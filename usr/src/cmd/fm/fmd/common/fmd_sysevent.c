/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)fmd_sysevent.c	1.6	06/02/11 SMI"

#include <sys/sysevent/eventdefs.h>
#include <sys/sysevent.h>
#include <sys/sysevent_impl.h>
#include <sys/fm/protocol.h>
#include <sys/sysmacros.h>
#include <sys/dumphdr.h>
#include <sys/dumpadm.h>
#include <sys/fm/util.h>

#include <libsysevent.h>
#include <libnvpair.h>
#include <alloca.h>
#include <limits.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#undef MUTEX_HELD
#undef RW_READ_HELD
#undef RW_WRITE_HELD

#include <fmd_api.h>
#include <fmd_log.h>
#include <fmd_subr.h>
#include <fmd_dispq.h>
#include <fmd_module.h>
#include <fmd_scheme.h>
#include <fmd_error.h>

#include <fmd.h>

static char *sysev_channel;	/* event channel to which we are subscribed */
static char *sysev_class;	/* event class to which we are subscribed */
static char *sysev_device;	/* device path to use for replaying events */
static char *sysev_sid;		/* event channel subscriber identifier */
static void *sysev_evc;		/* event channel cookie from evc_bind */

static fmd_xprt_t *sysev_xprt;
static fmd_hdl_t *sysev_hdl;

static struct sysev_stats {
	fmd_stat_t dump_replay;
	fmd_stat_t dump_lost;
	fmd_stat_t bad_class;
	fmd_stat_t bad_attr;
	fmd_stat_t eagain;
} sysev_stats = {
	{ "dump_replay", FMD_TYPE_UINT64, "events replayed from dump device" },
	{ "dump_lost", FMD_TYPE_UINT64, "events lost from dump device" },
	{ "bad_class", FMD_TYPE_UINT64, "events dropped due to invalid class" },
	{ "bad_attr", FMD_TYPE_UINT64, "events dropped due to invalid nvlist" },
	{ "eagain", FMD_TYPE_UINT64, "events retried due to low memory" },
};

/*
 * Receive an event from the SysEvent channel and post it to our transport.
 * Under extreme low-memory situations where we cannot event unpack the event,
 * we can request that SysEvent redeliver the event later by returning EAGAIN.
 * If we do this too many times, the kernel will drop the event.  Rather than
 * keeping state per-event, we simply attempt a garbage-collect, hoping that
 * enough free memory will be available by the time the event is redelivered.
 */
static int
sysev_recv(sysevent_t *sep, void *arg)
{
	uint64_t seq = sysevent_get_seq(sep);
	fmd_xprt_t *xp = arg;
	nvlist_t *nvl;
	hrtime_t hrt;

	if (strcmp(sysevent_get_class_name(sep), EC_FM) != 0) {
		fmd_hdl_error(sysev_hdl, "discarding event 0x%llx: unexpected"
		    " transport class %s\n", seq, sysevent_get_class_name(sep));
		sysev_stats.bad_class.fmds_value.ui64++;
		return (0);
	}

	if (sysevent_get_attr_list(sep, &nvl) != 0) {
		if (errno == EAGAIN || errno == ENOMEM) {
			fmd_modhash_tryapply(fmd.d_mod_hash, fmd_module_trygc);
			fmd_scheme_hash_trygc(fmd.d_schemes);
			sysev_stats.eagain.fmds_value.ui64++;
			return (EAGAIN);
		}

		fmd_hdl_error(sysev_hdl, "discarding event 0x%llx: missing "
		    "or invalid payload", seq);
		sysev_stats.bad_attr.fmds_value.ui64++;
		return (0);
	}

	sysevent_get_time(sep, &hrt);
	fmd_xprt_post(sysev_hdl, xp, nvl, hrt);
	return (0);
}

/*
 * Checksum algorithm used by the dump transport for verifying the content of
 * error reports saved on the dump device (copy of the kernel's checksum32()).
 */
static uint32_t
sysev_checksum(void *cp_arg, size_t length)
{
	uchar_t *cp, *ep;
	uint32_t sum = 0;

	for (cp = cp_arg, ep = cp + length; cp < ep; cp++)
		sum = ((sum >> 1) | (sum << 31)) + *cp;

	return (sum);
}

/*
 * Replay saved events from the dump transport.  This function is installed as
 * the timer callback and is called only once during the module's lifetime.
 */
/*ARGSUSED*/
static void
sysev_replay(fmd_hdl_t *hdl, id_t id, void *arg)
{
	char *dumpdev;
	off64_t off, off0;
	int fd, err;

	/*
	 * Determine the appropriate dump device to use for replaying pending
	 * error reports.  If the device property is NULL (default), we
	 * open and query /dev/dump to determine the current dump device.
	 */
	if ((dumpdev = sysev_device) == NULL) {
		if ((fd = open("/dev/dump", O_RDONLY)) == -1) {
			fmd_hdl_error(hdl, "failed to open /dev/dump "
			    "to locate dump device for event replay");
			return;
		}

		dumpdev = alloca(PATH_MAX);
		err = ioctl(fd, DIOCGETDEV, dumpdev);
		(void) close(fd);

		if (err == -1) {
			if (errno != ENODEV) {
				fmd_hdl_error(hdl, "failed to obtain "
				    "path to dump device for event replay");
			}
			return;
		}
	}

	if (strcmp(dumpdev, "/dev/null") == 0)
		return; /* return silently and skip replay for /dev/null */

	/*
	 * Open the appropriate device and then determine the offset of the
	 * start of the ereport dump region located at the end of the device.
	 */
	if ((fd = open64(dumpdev, O_RDWR | O_DSYNC)) == -1) {
		fmd_hdl_error(hdl, "failed to open dump transport %s "
		    "(pending events will not be replayed)", dumpdev);
		return;
	}

	off = DUMP_OFFSET + DUMP_LOGSIZE + DUMP_ERPTSIZE;
	off = off0 = lseek64(fd, -off, SEEK_END) & -DUMP_OFFSET;

	if (off == (off64_t)-1LL) {
		fmd_hdl_error(hdl, "failed to seek dump transport %s "
		    "(pending events will not be replayed)", dumpdev);
		(void) close(fd);
		return;
	}

	/*
	 * The ereport dump region is a sequence of erpt_dump_t headers each of
	 * which is followed by packed nvlist data.  We iterate over them in
	 * order, unpacking and dispatching each one to our dispatch queue.
	 */
	for (;;) {
		char nvbuf[ERPT_DATA_SZ];
		uint32_t chksum;
		erpt_dump_t ed;
		nvlist_t *nvl;

		fmd_timeval_t ftv, tod;
		hrtime_t hrt;
		uint64_t ena;

		if (pread64(fd, &ed, sizeof (ed), off) != sizeof (ed)) {
			fmd_hdl_error(hdl, "failed to read from dump "
			    "transport %s (pending events lost)", dumpdev);
			break;
		}

		if (ed.ed_magic == 0 && ed.ed_size == 0)
			break; /* end of list: all zero */

		if (ed.ed_magic == 0) {
			off += sizeof (ed) + ed.ed_size;
			continue; /* continue searching */
		}

		if (ed.ed_magic != ERPT_MAGIC) {
			/*
			 * Stop reading silently if the first record has the
			 * wrong magic number; this likely indicates that we
			 * rebooted from non-FMA bits or paged over the dump.
			 */
			if (off == off0)
				break;

			fmd_hdl_error(hdl, "invalid dump transport "
			    "record at %llx (magic number %x, expected %x)\n",
			    (u_longlong_t)off, ed.ed_magic, ERPT_MAGIC);
			break;
		}

		if (ed.ed_size > ERPT_DATA_SZ) {
			fmd_hdl_error(hdl, "invalid dump transport "
			    "record at %llx size (%u exceeds limit)\n",
			    (u_longlong_t)off, ed.ed_size);
			break;
		}

		if (pread64(fd, nvbuf, ed.ed_size,
		    off + sizeof (ed)) != ed.ed_size) {
			fmd_hdl_error(hdl, "failed to read dump "
			    "transport event (offset %llx)", (u_longlong_t)off);

			sysev_stats.dump_lost.fmds_value.ui64++;
			goto next;
		}

		if ((chksum = sysev_checksum(nvbuf,
		    ed.ed_size)) != ed.ed_chksum) {
			fmd_hdl_error(hdl, "dump transport event at "
			    "offset %llx is corrupt (checksum %x != %x)\n",
			    (u_longlong_t)off, chksum, ed.ed_chksum);

			sysev_stats.dump_lost.fmds_value.ui64++;
			goto next;
		}

		if ((err = nvlist_xunpack(nvbuf,
		    ed.ed_size, &nvl, &fmd.d_nva)) != 0) {
			fmd_hdl_error(hdl, "failed to unpack dump "
			    "transport event at offset %llx: %s\n",
			    (u_longlong_t)off, fmd_strerror(err));

			sysev_stats.dump_lost.fmds_value.ui64++;
			goto next;
		}

		/*
		 * If ed_hrt_nsec is set it contains the gethrtime() value from
		 * when the event was originally enqueued for the transport.
		 * If it is zero, we use the weaker bound ed_hrt_base instead.
		 */
		if (ed.ed_hrt_nsec != 0)
			hrt = ed.ed_hrt_nsec;
		else
			hrt = ed.ed_hrt_base;

		/*
		 * If this is an FMA protocol event of class "ereport.*" that
		 * contains valid ENA, we can improve the precision of 'hrt'.
		 */
		if (nvlist_lookup_uint64(nvl, FM_EREPORT_ENA, &ena) == 0)
			hrt = fmd_time_ena2hrt(hrt, ena);

		/*
		 * Now convert 'hrt' to an adjustable TOD based on the values
		 * in ed_tod_base which correspond to one another and are
		 * sampled before reboot using the old gethrtime() clock.
		 * fmd_event_recreate() will use this TOD value to re-assign
		 * the event an updated gethrtime() value based on the current
		 * value of the non-adjustable gethrtime() clock.  Phew.
		 */
		tod.ftv_sec = ed.ed_tod_base.sec;
		tod.ftv_nsec = ed.ed_tod_base.nsec;
		fmd_time_hrt2tod(ed.ed_hrt_base, &tod, hrt, &ftv);

		(void) nvlist_remove_all(nvl, FMD_EVN_TOD);
		(void) nvlist_add_uint64_array(nvl,
		    FMD_EVN_TOD, (uint64_t *)&ftv, 2);

		fmd_xprt_post(hdl, sysev_xprt, nvl, 0);
		sysev_stats.dump_replay.fmds_value.ui64++;

next:
		/*
		 * Reset the magic number for the event record to zero so that
		 * we do not replay the same event multiple times.
		 */
		ed.ed_magic = 0;

		if (pwrite64(fd, &ed, sizeof (ed), off) != sizeof (ed)) {
			fmd_hdl_error(hdl, "failed to mark dump "
			    "transport event (offset %llx)", (u_longlong_t)off);
		}

		off += sizeof (ed) + ed.ed_size;
	}

	(void) close(fd);
}

static const fmd_prop_t sysev_props[] = {
	{ "class", FMD_TYPE_STRING, EC_ALL },		/* event class */
	{ "device", FMD_TYPE_STRING, NULL },		/* replay device */
	{ "channel", FMD_TYPE_STRING, FM_ERROR_CHAN },	/* channel name */
	{ "sid", FMD_TYPE_STRING, "fmd" },		/* subscriber id */
	{ NULL, 0, NULL }
};

static const fmd_hdl_ops_t sysev_ops = {
	NULL,		/* fmdo_recv */
	sysev_replay,	/* fmdo_timeout */
	NULL,		/* fmdo_close */
	NULL,		/* fmdo_stats */
	NULL,		/* fmdo_gc */
	NULL,		/* fmdo_send */
};

static const fmd_hdl_info_t sysev_info = {
	"SysEvent Transport Agent", "1.0", &sysev_ops, sysev_props
};

/*
 * Bind to the sysevent channel we use for listening for error events and then
 * subscribe to appropriate events received over this channel.
 */
void
sysev_init(fmd_hdl_t *hdl)
{
	uint_t flags;

	if (fmd_hdl_register(hdl, FMD_API_VERSION, &sysev_info) != 0)
		return; /* invalid property settings */

	(void) fmd_stat_create(hdl, FMD_STAT_NOALLOC, sizeof (sysev_stats) /
	    sizeof (fmd_stat_t), (fmd_stat_t *)&sysev_stats);

	sysev_channel = fmd_prop_get_string(hdl, "channel");
	sysev_class = fmd_prop_get_string(hdl, "class");
	sysev_device = fmd_prop_get_string(hdl, "device");
	sysev_sid = fmd_prop_get_string(hdl, "sid");

	if (sysev_channel == NULL)
		fmd_hdl_abort(hdl, "channel property must be defined\n");

	if (sysev_sid == NULL)
		fmd_hdl_abort(hdl, "sid property must be defined\n");

	if ((errno = sysevent_evc_bind(sysev_channel, &sysev_evc,
	    EVCH_CREAT | EVCH_HOLD_PEND)) != 0) {
		fmd_hdl_abort(hdl, "failed to bind to event transport "
		    "channel %s", sysev_channel);
	}

	sysev_xprt = fmd_xprt_open(hdl, FMD_XPRT_RDONLY, NULL, NULL);
	sysev_hdl = hdl;

	/*
	 * If we're subscribing to the default channel, keep our subscription
	 * active even if we die unexpectedly so we continue queuing events.
	 * If we're not (e.g. running under fmsim), do not specify SUB_KEEP so
	 * that our event channel will be destroyed if we die unpleasantly.
	 */
	if (strcmp(sysev_channel, FM_ERROR_CHAN) == 0)
		flags = EVCH_SUB_KEEP | EVCH_SUB_DUMP;
	else
		flags = EVCH_SUB_DUMP;

	errno = sysevent_evc_subscribe(sysev_evc,
	    sysev_sid, sysev_class, sysev_recv, sysev_xprt, flags);

	if (errno != 0) {
		if (errno == EEXIST) {
			fmd_hdl_abort(hdl, "another fault management daemon is "
			    "active on transport channel %s\n", sysev_channel);
		} else {
			fmd_hdl_abort(hdl, "failed to subscribe to %s on "
			    "transport channel %s", sysev_class, sysev_channel);
		}
	}

	/*
	 * Once the transport is open, install a single timer to fire at once
	 * in the context of the module's thread to run sysev_replay().  This
	 * thread will block in its first fmd_xprt_post() until fmd is ready.
	 */
	fmd_hdl_debug(hdl, "transport '%s' open\n", sysev_channel);
	(void) fmd_timer_install(hdl, NULL, NULL, 0);
}

/*
 * Close the channel by unsubscribing and unbinding.  We only do this when a
 * a non-default channel has been selected.  If we're using FM_ERROR_CHAN,
 * the system default, we do *not* want to unsubscribe because the kernel will
 * remove the subscriber queue and any events published in our absence will
 * therefore be lost.  This scenario may occur when, for example, fmd is sent
 * a SIGTERM by init(1M) during reboot but an error is detected and makes it
 * into the sysevent channel queue before init(1M) manages to call uadmin(2).
 */
void
sysev_fini(fmd_hdl_t *hdl)
{
	if (strcmp(sysev_channel, FM_ERROR_CHAN) != 0) {
		sysevent_evc_unsubscribe(sysev_evc, sysev_sid);
		sysevent_evc_unbind(sysev_evc);
	}

	if (sysev_xprt != NULL)
		fmd_xprt_close(hdl, sysev_xprt);

	fmd_prop_free_string(hdl, sysev_class);
	fmd_prop_free_string(hdl, sysev_channel);
	fmd_prop_free_string(hdl, sysev_device);
	fmd_prop_free_string(hdl, sysev_sid);
}
