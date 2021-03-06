/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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

#pragma ident	"@(#)t10_sbc.c	1.2	06/07/10 SMI"

/*
 * []------------------------------------------------------------------[]
 * | Implementation of SBC-2 emulation					|
 * []------------------------------------------------------------------[]
 */
#include <sys/types.h>
#include <aio.h>
#include <sys/asynch.h>
#include <sys/mman.h>
#include <stddef.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>

#include <sys/scsi/generic/sense.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/generic/inquiry.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/mode.h>
#include <sys/scsi/generic/dad_mode.h>

#include "t10.h"
#include "t10_spc.h"
#include "t10_sbc.h"
#include "utility.h"

/*
 * Forward declarations
 */
static int sbc_mmap_overlap(const void *v1, const void *v2);
static void sbc_overlap_store(disk_io_t *io);
static void sbc_overlap_free(disk_io_t *io);
static void sbc_overlap_check(disk_io_t *io);
static void sbc_overlap_flush(disk_params_t *d);
static void sbc_cmd(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len);
static void sbc_data(t10_cmd_t *cmd, emul_handle_t e, size_t offset,
    char *data, size_t data_len);
static disk_io_t *sbc_io_alloc(t10_cmd_t *c);
static void sbc_io_free(emul_handle_t e);
static void sbc_read_cmplt(emul_handle_t e);
static void sbc_write_cmplt(emul_handle_t e);
static void sbc_read_capacity16(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len);
static char *sense_page3(disk_params_t *d, char *buf);
static char *sense_page4(disk_params_t *d, char *buf);
static char *sense_cache(disk_params_t *d, char *buf);
static char *sense_mode_control(t10_lu_impl_t *lu, char *buf);
static char *sense_info_ctrl(char *buf);
static scsi_cmd_table_t lba_table[];

/*
 * []----
 * | sbc_init_common -- Initialize LU data which is common to all I_T_Ls
 * []----
 */
Boolean_t
sbc_init_common(t10_lu_common_t *lu)
{
	disk_params_t	*d;
	xml_node_t	*node	= lu->l_root;

	if ((d = (disk_params_t *)calloc(1, sizeof (*d))) == NULL)
		return (False);

	(void) xml_find_value_int(node, XML_ELEMENT_BPS,
	    (int *)&d->d_bytes_sect);
	(void) xml_find_value_int(node, XML_ELEMENT_HEADS,
	    (int *)&d->d_heads);
	(void) xml_find_value_int(node, XML_ELEMENT_SPT,
	    (int *)&d->d_spt);
	(void) xml_find_value_int(node, XML_ELEMENT_CYLINDERS,
	    (int *)&d->d_cyl);
	(void) xml_find_value_int(node, XML_ELEMENT_RPM,
	    (int *)&d->d_rpm);
	(void) xml_find_value_int(node, XML_ELEMENT_INTERLEAVE,
	    (int *)&d->d_interleave);
	d->d_fast_write	= lu->l_fast_write_ack;
	d->d_size	= lu->l_size / (uint64_t)d->d_bytes_sect;
	d->d_state	= lu->l_state;

	avl_create(&d->d_mmap_overlaps, sbc_mmap_overlap,
	    sizeof (disk_io_t), offsetof(disk_io_t, da_mmap_overlap));
	(void) pthread_mutex_init(&d->d_mutex, NULL);
	(void) pthread_cond_init(&d->d_mmap_cond, NULL);
	(void) pthread_cond_init(&d->d_io_cond, NULL);
	if ((d->d_io_reserved = (disk_io_t *)calloc(1, sizeof (disk_io_t))) ==
	    NULL) {
		free(d);
		return (False);
	}

	lu->l_dtype_params = (void *)d;
	return (True);
}

void
sbc_fini_common(t10_lu_common_t *lu)
{
	disk_params_t	*d = lu->l_dtype_params;

	sbc_overlap_flush(d);
	avl_destroy(&d->d_mmap_overlaps);
	free(d->d_io_reserved);
	free(lu->l_dtype_params);
}

void
sbc_task_mgmt(t10_lu_common_t *lu, TaskOp_t op)
{
	disk_params_t	*d = (disk_params_t *)lu->l_dtype_params;

	switch (op) {
	case CapacityChange:
		d->d_size = lu->l_size / (uint64_t)d->d_bytes_sect;
		break;

	case DeviceOnline:
		d->d_state = lu->l_state;
		break;
	}
}

/*
 * []----
 * | sbc_init_per -- Initialize per I_T_L information
 * []----
 */
void
sbc_init_per(t10_lu_impl_t *itl)
{
	disk_params_t	*d = (disk_params_t *)itl->l_common->l_dtype_params;

	if (d->d_state == lu_online)
		itl->l_cmd	= sbc_cmd;
	else
		itl->l_cmd	= spc_cmd_offline;
	itl->l_data	= sbc_data;
	itl->l_cmd_table = lba_table;

}

void
sbc_fini_per(t10_lu_impl_t *itl)
{
	disk_params_t	*d = (disk_params_t *)itl->l_common->l_dtype_params;
	t10_lu_impl_t	*lu;

	if (d->d_reserve_owner == itl) {

		/*
		 * Since we currently own the reservation, drop it,
		 * and restore everyone elses command pointer.
		 */
		lu = avl_first(&itl->l_common->l_all_open);
		do {
			lu->l_cmd = sbc_cmd;
			lu = AVL_NEXT(&itl->l_common->l_all_open, lu);
		} while (lu != NULL);
		d->d_reserve_owner = NULL;
	}
}

/*
 * []----
 * | sbc_cmd -- start a SCSI command
 * |
 * | This routine is called from within the SAM-3 Task router.
 * []----
 */
static void
sbc_cmd(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	scsi_cmd_table_t	*e;

	e = &cmd->c_lu->l_cmd_table[cdb[0]];
#ifdef FULL_DEBUG
	queue_prt(mgmtq, Q_STE_IO, "SBC%x  LUN%d Cmd %s\n",
	    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
	    e->cmd_name == NULL ? "(no name)" : e->cmd_name);
#endif
	(*e->cmd_start)(cmd, cdb, cdb_len);
}

/*
 * []----
 * | sbc_cmd_reserve -- Run commands when another I_T_L has a reservation
 * []----
 */
static void
sbc_cmd_reserved(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	scsi_cmd_table_t	*e;

	switch (cdb[0]) {
	case SCMD_TEST_UNIT_READY:
	case SCMD_INQUIRY:
	case SCMD_REPORT_LUNS:
	case SCMD_LOG_SENSE_G1:
	case SCMD_READ_MEDIA_SERIAL:
	case SCMD_REPORT_TARGET_PORT_GROUPS:
	case SCMD_REQUEST_SENSE:
		/*
		 * SPC-2, revision 20, Section 5.5.1 table 10
		 * The specification allows these three commands
		 * to run even through there's a reservation in place.
		 */
		e = &cmd->c_lu->l_cmd_table[cdb[0]];
#ifdef FULL_DEBUG
		queue_prt(mgmtq, Q_STE_IO, "RESERVED: SBC%x  LUN%d Cmd %s\n",
		    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
		    e->cmd_name == NULL ? "(no name)" : e->cmd_name);
#endif
		(*e->cmd_start)(cmd, cdb, cdb_len);
		break;

	default:
		trans_send_complete(cmd, STATUS_RESERVATION_CONFLICT);
	}
}

/*
 * []----
 * | sbc_data -- Data phase for command.
 * |
 * | Normally this is only called for the WRITE command. Other commands
 * | that have a data in phase will probably be short circuited when
 * | we call trans_rqst_dataout() and the data is already available.
 * | At least this is true for iSCSI. FC however will need a DataIn phase
 * | for commands like MODE SELECT and PGROUT.
 * []----
 */
static void
sbc_data(t10_cmd_t *cmd, emul_handle_t id, size_t offset, char *data,
    size_t data_len)
{
	scsi_cmd_table_t	*e;

	e = &cmd->c_lu->l_cmd_table[cmd->c_cdb[0]];
#ifdef FULL_DEBUG
	queue_prt(mgmtq, Q_STE_IO, "SBC%x  LUN%d Data %s\n",
	    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
	    e->cmd_name);
#endif
	(*e->cmd_data)(cmd, id, offset, data, data_len);
}

/*
 * []------------------------------------------------------------------[]
 * | SCSI Block Commands - 2						|
 * | T10/1417-D								|
 * | The following functions implement the emulation of SBC-2 type	|
 * | commands.								|
 * []------------------------------------------------------------------[]
 */

/*
 * []----
 * | sbc_read -- emulation of SCSI READ command
 * []----
 */
/*ARGSUSED*/
static void
sbc_read(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	/*LINTED*/
	union scsi_cdb	*u		= (union scsi_cdb *)cdb;
	diskaddr_t	addr;
	off_t		offset		= 0;
	uint32_t	cnt,
			min;
	disk_io_t	*io;
	void		*mmap_data	= T10_MMAP_AREA(cmd);
	uint64_t	err_blkno;
	disk_params_t	*d;
	uchar_t		addl_sense_len;

	if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
		return;

	switch (u->scc_cmd) {
	case SCMD_READ:
		/*
		 * SBC-2 Revision 16, section 5.5
		 * Reserve bit checks
		 */
		if ((cdb[1] & 0xe0) || SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}

		addr = (diskaddr_t)(uint32_t)GETG0ADDR(u);
		cnt = GETG0COUNT(u);

		/*
		 * SBC-2 Revision 16
		 * Section: 5.5 READ(6) command
		 *	A TRANSFER LENGTH field set to zero specifies
		 *	that 256 logical blocks shall be read.
		 */
		if (cnt == 0)
			cnt = 256;
		break;

	case SCMD_READ_G1:
		/*
		 * SBC-2 Revision 16, section 5.6
		 * Reserve bit checks.
		 */
		if ((cdb[1] & 6) || cdb[6] ||
		    SAM_CONTROL_BYTE_RESERVED(cdb[9])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}

		addr = (diskaddr_t)(uint32_t)GETG1ADDR(u);
		cnt = GETG1COUNT(u);
		break;

	case SCMD_READ_G4:
		/*
		 * SBC-2 Revision 16, section 5.8
		 * Reserve bit checks
		 */
		if ((cdb[1] & 0x6) || cdb[14] ||
		    SAM_CONTROL_BYTE_RESERVED(cdb[15])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}

		addr = GETG4LONGADDR(u);
		cnt = GETG4COUNT(u);
		break;

	default:
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	if ((addr + cnt) > d->d_size) {

		if (addr > d->d_size)
			err_blkno = addr;
		else
			err_blkno = d->d_size;

		/*
		 * XXX: What's SBC-2 say about ASC/ASCQ here. Solaris
		 * doesn't care about these values when key is set
		 * to KEY_ILLEGAL_REQUEST.
		 */
		if (err_blkno > FIXED_SENSE_ADDL_INFO_LEN)
			addl_sense_len = INFORMATION_SENSE_DESCR;
		else
			addl_sense_len = 0;

		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, addl_sense_len);
		spc_sense_info(cmd, err_blkno);
		spc_sense_ascq(cmd, 0x21, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);

		queue_prt(mgmtq, Q_STE_ERRS,
		    "SBC%x  LUN%d READ Illegal sector "
		    "(0x%llx + 0x%x) > 0x%ullx", cmd->c_lu->l_targ->s_targ_num,
		    cmd->c_lu->l_common->l_num, addr, cnt, d->d_size);
		return;
	}

	cmd->c_lu->l_cmds_read++;
	cmd->c_lu->l_sects_read += cnt;

	if (cnt == 0) {
		trans_send_complete(cmd, STATUS_GOOD);
		return;
	}

	do {
		io = sbc_io_alloc(cmd);
		min = MIN((cnt * 512) - offset, T10_MAX_OUT(cmd));

		io->da_lba	= addr;
		io->da_lba_cnt	= cnt;
		io->da_offset	= offset;
		io->da_data_len	= min;

#ifdef FULL_DEBUG
		queue_prt(mgmtq, Q_STE_IO,
		    "SBC%x  LUN%d blk 0x%llx, cnt %d, offset 0x%llx, size %d",
		    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
		    addr, cnt, io->da_offset, min);
#endif
		if (mmap_data != MAP_FAILED) {

			io->da_clear_overlap		= True;
			io->da_data_alloc		= False;
			io->da_aio.a_aio.aio_return	= min;
			io->da_data = (char *)mmap_data + (addr * 512LL) +
			    io->da_offset;
			sbc_overlap_store(io);
			sbc_read_cmplt((emul_handle_t)io);

		} else {
			if ((io->da_data = (char *)malloc(min)) == NULL) {
				trans_send_complete(cmd, STATUS_BUSY);
				return;
			}
			io->da_clear_overlap	= False;
			io->da_data_alloc	= True;
			io->da_aio.a_aio_cmplt	= sbc_read_cmplt;
			io->da_aio.a_id		= io;
			trans_aioread(cmd, io->da_data, min, (addr * 512LL) +
			    (off_t)io->da_offset, (aio_result_t *)io);
		}
		offset += min;
	} while (offset < (off_t)(cnt * 512));
}

/*
 * []----
 * | sbc_read_cmplt -- Once we have the data, need to send it along.
 * []----
 */
static void
sbc_read_cmplt(emul_handle_t id)
{
	disk_io_t	*io		= (disk_io_t *)id;
	int		sense_len;
	uint64_t	err_blkno;
	t10_cmd_t	*cmd		= io->da_cmd;
	Boolean_t	last;

	if (io->da_aio.a_aio.aio_return != io->da_data_len) {
		err_blkno = io->da_lba + ((io->da_offset + 511) / 512);
		cmd->c_resid = (io->da_lba_cnt * 512) - io->da_offset;
		if (err_blkno > FIXED_SENSE_ADDL_INFO_LEN)
			sense_len = INFORMATION_SENSE_DESCR;
		else
			sense_len = 0;
		spc_sense_create(cmd, KEY_HARDWARE_ERROR, sense_len);
		spc_sense_info(cmd, err_blkno);
		trans_send_complete(cmd, STATUS_CHECK);
		sbc_io_free(io);
		return;
	}

	last = (io->da_offset + io->da_data_len) < (io->da_lba_cnt * 512LL) ?
	    False : True;
	if (trans_send_datain(cmd, io->da_data, io->da_data_len, io->da_offset,
	    sbc_io_free, last, io) == False) {
		trans_send_complete(cmd, STATUS_BUSY);
	}
}

/*
 * []----
 * | sbc_write -- implement a SCSI write command.
 * []----
 */
/*ARGSUSED*/
static void
sbc_write(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	union scsi_cdb	*u;
	diskaddr_t	addr;
	uint64_t	err_blkno;
	uint32_t	cnt;
	uchar_t		addl_sense_len;
	disk_params_t	*d;
	disk_io_t	*io;
	size_t		max_out;
	void		*mmap_area;

	if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
		return;

	/*LINTED*/
	u = (union scsi_cdb *)cdb;

	switch (u->scc_cmd) {
	case SCMD_WRITE:
		/*
		 * SBC-2 revision 16, section 5.24
		 * Reserve bit checks.
		 */
		if ((cdb[1] & 0xe0) || SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}
		addr = (diskaddr_t)(uint32_t)GETG0ADDR(u);
		cnt = GETG0COUNT(u);
		/*
		 * SBC-2 Revision 16/Section 5.24 WRITE(6)
		 * A TRANSFER LENGHT of 0 indicates that 256 logical blocks
		 * shall be written.
		 */
		if (cnt == 0)
			cnt = 256;
		break;

	case SCMD_WRITE_G1:
		/*
		 * SBC-2 revision 16, section 5.25
		 * Reserve bit checks.
		 */
		if ((cdb[1] & 0x6) || cdb[6] ||
		    SAM_CONTROL_BYTE_RESERVED(cdb[9])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}
		addr = (diskaddr_t)(uint32_t)GETG1ADDR(u);
		cnt = GETG1COUNT(u);
		break;

	case SCMD_WRITE_G4:
		/*
		 * SBC-2 revision 16, section 5.27
		 * Reserve bit checks.
		 */
		if ((cdb[1] & 0x6) || cdb[14] ||
		    SAM_CONTROL_BYTE_RESERVED(cdb[15])) {
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}
		addr = (diskaddr_t)GETG4LONGADDR(u);
		cnt = GETG4COUNT(u);
		break;

	default:
		queue_prt(mgmtq, Q_STE_ERRS, "Unprocessed WRITE type");
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;

	}

	if ((addr + cnt) > d->d_size) {

		if (addr > d->d_size)
			err_blkno = addr;
		else
			err_blkno = d->d_size;

		/*
		 * XXX: What's SBC-2 say about ASC/ASCQ here. Solaris
		 * doesn't care about these values when key is set
		 * to KEY_ILLEGAL_REQUEST.
		 */
		if (err_blkno > FIXED_SENSE_ADDL_INFO_LEN)
			addl_sense_len = INFORMATION_SENSE_DESCR;
		else
			addl_sense_len = 0;

		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, addl_sense_len);
		spc_sense_info(cmd, err_blkno);
		spc_sense_ascq(cmd, 0x21, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);

		queue_prt(mgmtq, Q_STE_ERRS,
		    "SBC%x  LUN%d WRITE Illegal sector "
		    "(0x%llx + 0x%x) > 0x%ullx", cmd->c_lu->l_targ->s_targ_num,
		    cmd->c_lu->l_common->l_num, addr, cnt, d->d_size);
		return;
	}

	if (cnt == 0) {
		queue_prt(mgmtq, Q_STE_NONIO,
		    "SBC%x  LUN%d WRITE zero block count for addr 0x%x",
		    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
		    addr);
		trans_send_complete(cmd, STATUS_GOOD);
		return;
	}

	io = (disk_io_t *)cmd->c_emul_id;
	if (io == NULL) {
		io = sbc_io_alloc(cmd);
		io->da_lba		= addr;
		io->da_lba_cnt		= cnt;
		io->da_clear_overlap	= False;
		io->da_aio.a_aio_cmplt	= sbc_write_cmplt;
		io->da_aio.a_id		= io;

		/*
		 * Only update the statistics the first time through
		 * for this particular command. If the requested transfer
		 * is larger than the transport can handle this routine
		 * will be called many times.
		 */
		cmd->c_lu->l_cmds_write++;
		cmd->c_lu->l_sects_write += cnt;
	}

#ifdef FULL_DEBUG
	queue_prt(mgmtq, Q_STE_IO,
	    "SBC%x  LUN%d blk 0x%llx, cnt %d, offset 0x%llx, size %d",
	    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num, addr,
	    cnt, io->da_offset, io->da_data_len);
#endif

	/*
	 * If a transport sets the maximum output value to zero we'll
	 * just request the entire amount. Otherwise, transfer no more
	 * than the maximum output or the reminder, whichever is less.
	 */
	max_out = cmd->c_lu->l_targ->s_maxout;
	io->da_data_len = max_out ? MIN(max_out,
	    (cnt * 512) - io->da_offset) : (cnt * 512);

	mmap_area = T10_MMAP_AREA(cmd);
	if (mmap_area != MAP_FAILED) {

		io->da_data_alloc	= False;
		io->da_data		= (char *)mmap_area + (addr * 512LL) +
		    io->da_offset;
		sbc_overlap_check(io);

	} else if ((io->da_data = (char *)malloc(io->da_data_len)) == NULL) {

		trans_send_complete(cmd, STATUS_BUSY);
		return;

	} else {

		io->da_data_alloc	= True;
	}
	if (trans_rqst_dataout(cmd, io->da_data, io->da_data_len,
	    io->da_offset, io) == False) {
		trans_send_complete(cmd, STATUS_BUSY);
	}
}

/*
 * []----
 * | sbc_write_data -- store a chunk of data from the transport
 * []----
 */
/*ARGSUSED*/
void
sbc_write_data(t10_cmd_t *cmd, emul_handle_t id, size_t offset, char *data,
    size_t data_len)
{
	disk_io_t	*io = (disk_io_t *)id;
	disk_params_t	*d;

	if (cmd->c_lu->l_common->l_mmap == MAP_FAILED) {
		trans_aiowrite(cmd, data, data_len, (io->da_lba * 512) +
		    (off_t)io->da_offset, (aio_result_t *)io);
	} else {
		if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
			return;

		if (d->d_fast_write == False) {

			/*
			 * We only need to worry about sync'ing the blocks
			 * in the mmap case because if the fast cache isn't
			 * enabled for AIO the file will be opened with F_SYNC
			 * which performs the correct action.
			 */
			if (fsync(cmd->c_lu->l_common->l_fd) == -1) {
				spc_sense_create(cmd, KEY_HARDWARE_ERROR, 0);
				trans_send_complete(cmd, STATUS_CHECK);
				return;
			}
		}

		/*
		 * Since the data has already been transfered from the
		 * transport to the mmap area we just need to call
		 * the complete routine.
		 */
		sbc_write_cmplt(id);
	}
}

/*
 * []----
 * | sbc_write_cmplt -- deal with end game of write
 * |
 * | See if all of the data for this write operation has been dealt
 * | with. If so, send a final acknowledgement back to the transport.
 * | If not, update the offset, calculate the next transfer size, and
 * | start the process again.
 * []---
 */
static void
sbc_write_cmplt(emul_handle_t e)
{
	disk_io_t	*io	= (disk_io_t *)e;
	t10_cmd_t	*cmd	= io->da_cmd;

	if ((io->da_offset + io->da_data_len) < (io->da_lba_cnt * 512)) {
		if (io->da_data_alloc == True) {
			io->da_data_alloc = False;
			free(io->da_data);
		}

		io->da_offset	+= io->da_data_len;
		io->da_data_len	= MIN(cmd->c_lu->l_targ->s_maxout,
		    (io->da_lba_cnt * 512) - io->da_offset);
		sbc_write(cmd, cmd->c_cdb, cmd->c_cdb_len);
		return;
	}
	sbc_io_free(io);
	trans_send_complete(cmd, STATUS_GOOD);
}

/*ARGSUSED*/
void
sbc_startstop(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	/*
	 * SBC-2 revision 16, section 5.17
	 * Reserve bit checks
	 */
	if ((cdb[1] & 0xfe) || cdb[2] || cdb[3] ||
	    (cdb[4] & ~(SBC_PWR_MASK|SBC_PWR_LOEJ|SBC_PWR_START)) ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	/*
	 * More reserve bit checks
	 */
	switch ((cdb[4] & SBC_PWR_MASK) >> SBC_PWR_SHFT) {
		case SBC_PWR_START_VALID:
			/*
			 * It's an error to ask that the media be ejected.
			 *
			 * NOTE: Look for method to pass the START bit
			 * along to underlying storage. If we're asked to
			 * stop the drive there's not much that we can do
			 * for the virtual storage, but maybe everything else
			 * has been requested to stop as well.
			 */
			if (cdb[4] & SBC_PWR_LOEJ) {
				goto send_error;
			}
			break;

		case SBC_PWR_ACTIVE:
		case SBC_PWR_IDLE:
		case SBC_PWR_STANDBY:
		case SBC_PWR_OBSOLETE:
			break;

		case SBC_PWR_LU_CONTROL:
		case SBC_PWR_FORCE_IDLE_0:
		case SBC_PWR_FORCE_STANDBY_0:
			break;

		default:
send_error:
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
	}

	if ((cdb[1] & 1) == 0) {
		/*
		 * Immediate bit is not set, so go ahead a flush things.
		 */
		if (fsync(cmd->c_lu->l_common->l_fd) != 0) {
			spc_sense_create(cmd, KEY_MEDIUM_ERROR, 0);
			trans_send_complete(cmd, STATUS_CHECK);
			return;
		}
	}
	trans_send_complete(cmd, STATUS_GOOD);
}

/*
 * []----
 * | sbc_recap -- read capacity of device being emulated.
 * []----
 */
/*ARGSUSED*/
void
sbc_recap(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	uint64_t			capacity;
	int				len;
	uint32_t			lba;
	struct scsi_capacity *cap;
	disk_params_t			*d;
	disk_io_t			*io;

	if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
		return;

	capacity = d->d_size;

	len = sizeof (struct scsi_capacity);

	/*
	 * SBC-2 Revision 16, section 5.10.1
	 * Any of the following conditions will generate an error.
	 *    (1) PMI bit is zero and LOGICAL block address is non-zero
	 *    (2) Rserved bytes are not zero
	 *    (3) Reseved bits are not zero
	 *    (4) Reserved CONTROL bits are not zero
	 */
	if ((((cdb[8] & SBC_CAPACITY_PMI) == 0) &&
	    (cdb[2] || cdb[3] || cdb[4] || cdb[5])) ||
	    cdb[1] || cdb[6] || cdb[7] || (cdb[8] & ~SBC_CAPACITY_PMI) ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[9])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	/*
	 * if the device capacity larger than 32 bits then set
	 * the capacity of the device to all 0xf's.
	 * a device that supports LBAs larger than 32 bits which
	 * should be used read_capacity(16) comand to get the capacity.
	 * NOTE: the adjustment to subject one from the capacity is
	 * done below.
	 */
	if (capacity & 0xFFFFFFFF00000000ULL)
		capacity = 0xFFFFFFFF;

	io = sbc_io_alloc(cmd);

	if ((cap = (struct scsi_capacity *)calloc(1, len)) == NULL) {
		sbc_io_free(io);
		trans_send_complete(cmd, STATUS_BUSY);
		return;
	}
	io->da_data		= (char *)cap;
	io->da_data_alloc	= True;
	io->da_clear_overlap	= False;
	io->da_data_len		= len;

	if (capacity != 0xFFFFFFFF) {
		/*
		 * Look at the PMI information
		 */
		if (cdb[8] & SBC_CAPACITY_PMI) {
			lba = cdb[2] << 24 | cdb[3] << 16 |
			    cdb[4] << 8 | cdb[5];
			if (lba >= capacity)
				cap->capacity = htonl(0xffffffff);
			else
				cap->capacity = (capacity - 1);
		} else {
			cap->capacity = htonl(capacity - 1);
		}
	} else {
		cap->capacity = htonl(capacity);
	}
	cap->lbasize = htonl(d->d_bytes_sect);

	if (trans_send_datain(cmd, io->da_data, io->da_data_len, 0,
	    sbc_io_free, True, io) == False) {
		trans_send_complete(cmd, STATUS_BUSY);
	}
}

/*ARGSUSED*/
void
sbc_msense(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	struct mode_header	*mode_hdr;
	char			*np;
	disk_params_t		*d;
	disk_io_t		*io;

	if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
		return;

	/*
	 * SPC-3 Revision 21c section 6.8
	 * Reserve bit checks
	 */
	if ((cdb[1] & ~SPC_MODE_SENSE_DBD) ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	/*
	 * Zero length causes a simple ack to occur.
	 */
	if (cdb[4] == 0) {
		trans_send_complete(cmd, STATUS_GOOD);
		return;
	}

	io = sbc_io_alloc(cmd);

	/*
	 * Make sure that we have enough room in the data buffer. We'll
	 * only send back the amount requested though
	 */
	io->da_data_len = MAX(cdb[4], sizeof (struct mode_format) +
	    sizeof (struct mode_geometry) +
	    sizeof (struct mode_control_scsi3) +
	    sizeof (struct mode_cache_scsi3) +
	    sizeof (struct mode_info_ctrl) + (MODE_BLK_DESC_LENGTH * 5));
	if ((io->da_data = (char *)calloc(1, io->da_data_len)) == NULL) {
		sbc_io_free(io);
		trans_send_complete(cmd, STATUS_BUSY);
		return;
	}
	io->da_clear_overlap	= False;
	io->da_data_alloc	= True;
	mode_hdr		= (struct mode_header *)io->da_data;

	switch (cdb[2]) {
	case MODE_SENSE_PAGE3_CODE:
		if ((d->d_heads == 0) && (d->d_cyl == 0) && (d->d_spt == 0)) {
			sbc_io_free(io);
			spc_unsupported(cmd, cdb, cdb_len);
			return;
		}
		mode_hdr->length	= sizeof (struct mode_format);
		mode_hdr->bdesc_length	= MODE_BLK_DESC_LENGTH;
		(void) sense_page3(d,
		    io->da_data + sizeof (*mode_hdr) + mode_hdr->bdesc_length);

		break;

	case MODE_SENSE_PAGE4_CODE:
		if ((d->d_heads == 0) && (d->d_cyl == 0) && (d->d_spt == 0)) {
			sbc_io_free(io);
			spc_unsupported(cmd, cdb, cdb_len);
			return;
		}
		mode_hdr->length	= sizeof (struct mode_geometry);
		mode_hdr->bdesc_length	= MODE_BLK_DESC_LENGTH;
		(void) sense_page4(d,
		    io->da_data + sizeof (*mode_hdr) + mode_hdr->bdesc_length);
		break;

	case MODE_SENSE_CACHE:
		mode_hdr->length	= sizeof (struct mode_cache_scsi3);
		mode_hdr->bdesc_length	= MODE_BLK_DESC_LENGTH;
		(void) sense_cache(d,
		    io->da_data + sizeof (*mode_hdr) + mode_hdr->bdesc_length);
		break;

	case MODE_SENSE_CONTROL:
		mode_hdr->length	= sizeof (struct mode_control_scsi3);
		mode_hdr->bdesc_length	= MODE_BLK_DESC_LENGTH;
		(void) sense_mode_control(cmd->c_lu,
		    io->da_data + sizeof (*mode_hdr) + mode_hdr->bdesc_length);
		break;

	case MODE_SENSE_INFO_CTRL:
		(void) sense_info_ctrl(io->da_data);
		break;

	case MODE_SENSE_SEND_ALL:
		/*
		 * SPC-3 revision 21c
		 * Section 6.9.1 Table 97
		 * "Return all subpage 00h mode pages in page_0 format"
		 */
		if (io->da_data_len < (sizeof (struct mode_format) +
		    sizeof (struct mode_geometry) +
		    sizeof (struct mode_control_scsi3) +
		    sizeof (struct mode_info_ctrl))) {

			/*
			 * Believe it or not, there's an initiator out
			 * there which sends a mode sense request for all
			 * of the pages, without always sending a data-in
			 * size which is large enough.
			 * NOTE: Need to check the error key returned
			 * here and see if something else should be used.
			 */
			spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
			trans_send_complete(cmd, STATUS_CHECK);

		} else {

			/*
			 * If we don't have geometry then don't attempt
			 * report that information.
			 */
			if (d->d_heads && d->d_cyl && d->d_spt) {
				np = sense_page3(d, io->da_data);
				np = sense_page4(d, np);
			}
			np = sense_cache(d, np);
			np = sense_mode_control(cmd->c_lu, np);
			(void) sense_info_ctrl(np);

		}
		break;

	case 0x00:
		/*
		 * SPC-3 Revision 21c, section 6.9.1
		 * Table 97 -- Mode page code usage for all devices
		 * Page Code 00 == Vendor specific. We are going to return
		 *    zeros.
		 */
		break;

	default:
		queue_prt(mgmtq, Q_STE_ERRS,
		    "SBC%x  LUN%d Unsupported mode_sense request 0x%x",
		    cmd->c_lu->l_targ->s_targ_num, cmd->c_lu->l_common->l_num,
		    cdb[2]);
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
		break;
	}

	if (trans_send_datain(cmd, io->da_data, cdb[4], 0, sbc_io_free,
	    True, io) == False) {
		trans_send_complete(cmd, STATUS_BUSY);
	}
}

/*ARGSUSED*/
void
sbc_synccache(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	/*
	 * SBC-2 revision 16, section 5.18
	 * Reserve bit checks
	 */
	if ((cdb[1] & ~SBC_SYNC_CACHE_IMMED) || cdb[6] ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[9])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
	} else {
		/*
		 * SBC-3, revision 16, section 5.18
		 * An IMMED bit set to one specifies that the device server
		 * shall return status as soon as the CDB has been validated.
		 */
		if (cdb[1] & SBC_SYNC_CACHE_IMMED) {

			/*
			 * Immediately return a status of GOOD. If an error
			 * occurs with the fsync the next command will pick
			 * up an error.
			 */
			trans_send_complete(cmd, STATUS_GOOD);
			if (fsync(cmd->c_lu->l_common->l_fd) == -1) {
				cmd->c_lu->l_status	= KEY_HARDWARE_ERROR;
				cmd->c_lu->l_asc	= 0x00;
				cmd->c_lu->l_ascq	= 0x00;
			}
		} else {
			if (fsync(cmd->c_lu->l_common->l_fd) == -1) {
				spc_sense_create(cmd, KEY_HARDWARE_ERROR, 0);
				trans_send_complete(cmd, STATUS_CHECK);
			} else
				trans_send_complete(cmd, STATUS_GOOD);
		}
	}
}

/*ARGSUSED*/
void
sbc_service_actiong4(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	switch (cdb[1] & SPC_GROUP4_SERVICE_ACTION_MASK) {
	case SSVC_ACTION_READ_CAPACITY_G4:
		sbc_read_capacity16(cmd, cdb, cdb_len);
		break;
	default:
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, 0x20, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		break;
	}
}

/*ARGSUSED*/
static void
sbc_read_capacity16(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	uint64_t		capacity,
				lba;
	int			rep_size;	/* response data size */
	struct scsi_capacity_16	*cap16;
	disk_params_t		*d;
	disk_io_t		*io;

	if ((d = (disk_params_t *)T10_PARAMS_AREA(cmd)) == NULL)
		return;

	capacity = d->d_size;
	/*
	 * READ_CAPACITY(16) command
	 */
	rep_size = cdb[10] << 24 | cdb[11] << 16 | cdb[12] << 8 | cdb[13];
	if (rep_size == 0) {

		/*
		 * A zero length field means we're done.
		 */
		trans_send_complete(cmd, STATUS_GOOD);
		return;
	}
	rep_size = MIN(rep_size, sizeof (*cap16));

	/*
	 * Reserve bit checks.
	 */
	if ((cdb[1] & ~SPC_GROUP4_SERVICE_ACTION_MASK) ||
	    (cdb[14] & ~SBC_CAPACITY_PMI) ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[15])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	lba = (uint64_t)cdb[2] << 56 | (uint64_t)cdb[3] << 48 |
		(uint64_t)cdb[4] << 40 | (uint64_t)cdb[5] << 32 |
		(uint64_t)cdb[6] << 24 | (uint64_t)cdb[7] << 16 |
		(uint64_t)cdb[8] << 8 | (uint64_t)cdb[9];

	io = sbc_io_alloc(cmd);

	/*
	 * We'll malloc enough space for the structure so that we can
	 * set the values as we place. However, we'll set the transfer
	 * length to the minimum of the requested size and our structure.
	 * This is per SBC-2 revision 16, section 5.11.1 regarding
	 * ALLOCATION LENGTH.
	 */
	if ((cap16 = (struct scsi_capacity_16 *)calloc(1, sizeof (*cap16))) ==
	    NULL) {
		trans_send_complete(cmd, STATUS_BUSY);
		return;
	}
	io->da_data		= (char *)cap16;
	io->da_data_len		= sizeof (*cap16);
	io->da_data_alloc	= True;
	io->da_clear_overlap	= False;

	if (cdb[14] & SBC_CAPACITY_PMI) {
		if (lba >= capacity)
			cap16->sc_capacity = htonll(0xffffffffffffffffULL);
		else
			cap16->sc_capacity = htonll(capacity - 1);
	} else {
		cap16->sc_capacity	= htonll(capacity - 1);
	}
	cap16->sc_lbasize	= htonl(d->d_bytes_sect);

	if (trans_send_datain(cmd, io->da_data, io->da_data_len, 0,
	    sbc_io_free, True, io) == False) {
		trans_send_complete(cmd, STATUS_BUSY);
	}
}

/*ARGSUSED*/
static void
sbc_reserve(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	disk_params_t	*p = (disk_params_t *)T10_PARAMS_AREA(cmd);
	t10_lu_impl_t	*lu;

	if (p == NULL)
		return;

	if (cdb[1] & 0xe0 || SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	if ((p->d_reserve_owner != NULL) &&
	    (p->d_reserve_owner != cmd->c_lu)) {

		trans_send_complete(cmd, STATUS_RESERVATION_CONFLICT);
		return;

	} else if (p->d_reserve_owner == cmd->c_lu) {

		/*
		 * According SPC-2 revision 20, section 7.21.2
		 * It shall be permissible for an initiator to
		 * reserve a logic unit that is currently reserved
		 * by that initiator
		 */
		trans_send_complete(cmd, STATUS_GOOD);
	} else {

		lu = avl_first(&cmd->c_lu->l_common->l_all_open);
		do {
			if (lu != cmd->c_lu)
				lu->l_cmd = sbc_cmd_reserved;
			lu = AVL_NEXT(&cmd->c_lu->l_common->l_all_open, lu);
		} while (lu != NULL);
		p->d_reserve_owner = cmd->c_lu;
		trans_send_complete(cmd, STATUS_GOOD);
	}
}

/*ARGSUSED*/
static void
sbc_release(t10_cmd_t *cmd, uint8_t *cdb, size_t cdb_len)
{
	disk_params_t	*p = (disk_params_t *)T10_PARAMS_AREA(cmd);
	t10_lu_impl_t	*lu;

	if (p == NULL)
		return;

	if (cdb[1] & 0xe0 || cdb[3] || cdb[4] ||
	    SAM_CONTROL_BYTE_RESERVED(cdb[5])) {
		spc_sense_create(cmd, KEY_ILLEGAL_REQUEST, 0);
		spc_sense_ascq(cmd, SPC_ASC_INVALID_CDB, 0x00);
		trans_send_complete(cmd, STATUS_CHECK);
		return;
	}

	if (p->d_reserve_owner == NULL) {

		/*
		 * If nobody is the owner this command is successful.
		 */
		trans_send_complete(cmd, STATUS_GOOD);
		return;
	}

	/*
	 * At this point the only way to get in here is to be the owner
	 * of the reservation.
	 */
	lu = avl_first(&cmd->c_lu->l_common->l_all_open);
	do {
		lu->l_cmd = sbc_cmd;
		lu = AVL_NEXT(&cmd->c_lu->l_common->l_all_open, lu);
	} while (lu != NULL);
	p->d_reserve_owner = NULL;
	trans_send_complete(cmd, STATUS_GOOD);
}

/*
 * []------------------------------------------------------------------[]
 * | Support related functions for SBC-2				|
 * []------------------------------------------------------------------[]
 */

/*
 * []----
 * | sense_page3 -- Create page3 sense code for Disk.
 * |
 * | This is a separate routine because this is called in two different
 * | locations.
 * []----
 */
static char *
sense_page3(disk_params_t *d, char *buf)
{
	struct mode_format		mode_fmt;

	bzero(&mode_fmt, sizeof (mode_fmt));
	mode_fmt.mode_page.code		= MODE_SENSE_PAGE3_CODE;
	mode_fmt.mode_page.length	= sizeof (struct mode_format) -
	    sizeof (struct mode_page);
	mode_fmt.data_bytes_sect	= htons(d->d_bytes_sect);
	mode_fmt.sect_track		= htons(d->d_spt);
	mode_fmt.interleave		= htons(d->d_interleave);
	bcopy(&mode_fmt, buf, sizeof (mode_fmt));

	return (buf + sizeof (mode_fmt));
}

/*
 * []----
 * | sense_page4 -- Create page4 sense code for Disk.
 * |
 * | This is a separate routine because this is called in two different
 * | locations.
 * []----
 */
static char *
sense_page4(disk_params_t *d, char *buf)
{
	struct mode_geometry	mode_geom;

	bzero(&mode_geom, sizeof (mode_geom));
	mode_geom.mode_page.code	= MODE_SENSE_PAGE4_CODE;
	mode_geom.mode_page.length	=
	    sizeof (struct mode_geometry) - sizeof (struct mode_page);
	mode_geom.heads			= d->d_heads;
	mode_geom.cyl_ub		= d->d_cyl >> 16;
	mode_geom.cyl_mb		= d->d_cyl >> 8;
	mode_geom.cyl_lb		= d->d_cyl;
	mode_geom.rpm			= htons(d->d_rpm);
	bcopy(&mode_geom, buf, sizeof (mode_geom));

	return (buf + sizeof (mode_geom));
}

static char *
sense_cache(disk_params_t *d, char *buf)
{
	struct mode_cache_scsi3		mode_cache;

	bzero(&mode_cache, sizeof (mode_cache));

	mode_cache.wce = d->d_fast_write == True ? 1 : 0;
	bcopy(&mode_cache, buf, sizeof (mode_cache));

	return (buf + sizeof (mode_cache));
}

/*
 * []----
 * | sense_mode_control -- Create mode control page for disk
 * []----
 */
static char *
sense_mode_control(t10_lu_impl_t *lu, char *buf)
{
	struct mode_control_scsi3	m;

	bzero(&m, sizeof (m));
	m.mode_page.code	= MODE_SENSE_CONTROL;
	m.mode_page.length	= sizeof (struct mode_control_scsi3) -
		sizeof (struct mode_page);
	m.d_sense		= (lu->l_dsense_enabled == True) ? 1 : 0;
	bcopy(&m, buf, sizeof (m));

	return (buf + sizeof (m));
}

/*
 * []----
 * | sense_info_ctrl -- Create mode information control page
 * []----
 */
static char *
sense_info_ctrl(char *buf)
{
	struct mode_info_ctrl	info;

	bzero(&info, sizeof (info));
	info.mode_page.code	= 0x1c;
	info.mode_page.length	= sizeof (struct mode_info_ctrl) -
	    sizeof (struct mode_page);
	bcopy(&info, buf, sizeof (info));

	return (buf + sizeof (info));
}

/*
 * []----
 * | sbc_io_alloc -- return a disk_io_t structure
 * |
 * | If the call to calloc fails we use the structure that was allocate
 * | during the initial common initialization call. This will allow the
 * | daemon to at least make progress.
 * []----
 */
static disk_io_t *
sbc_io_alloc(t10_cmd_t *c)
{
	disk_io_t	*io;
	disk_params_t	*d = T10_PARAMS_AREA(c);

	if ((io = (disk_io_t *)calloc(1, sizeof (*io))) == NULL) {
		(void) pthread_mutex_lock(&d->d_mutex);
		if (d->d_io_used == True) {
			d->d_io_need = True;
			while (d->d_io_used == True)
				pthread_cond_wait(&d->d_io_cond, &d->d_mutex);
			d->d_io_need = False;
		}
		d->d_io_used	= True;
		io		= d->d_io_reserved;
		(void) pthread_mutex_unlock(&d->d_mutex);
	}

	io->da_cmd	= c;
	io->da_params	= d;

	return (io);
}

/*
 * []----
 * | sbc_io_free -- free local i/o buffers when transport is finished
 * |
 * | If the io structure being free is the preallocated buffer see if
 * | anyone is waiting for the buffer. If so, wake them up.
 * []----
 */
static void
sbc_io_free(emul_handle_t e)
{
	disk_io_t	*io = (disk_io_t *)e;

	if (io->da_clear_overlap == True)
		sbc_overlap_free(io);

	if (io->da_data_alloc == True)
		free(io->da_data);

	if (io == io->da_params->d_io_reserved) {
		(void) pthread_mutex_lock(&io->da_params->d_mutex);
		io->da_params->d_io_used = False;
		if (io->da_params->d_io_need == True)
			pthread_cond_signal(&io->da_params->d_io_cond);
		(void) pthread_mutex_unlock(&io->da_params->d_mutex);
	} else {
		free(io);
	}
}

static int
sbc_mmap_overlap(const void *v1, const void *v2)
{
	disk_io_t	*d1	= (disk_io_t *)v1,
			*d2	= (disk_io_t *)v2;

	if ((d1->da_data + d1->da_data_len) < d2->da_data)
		return (-1);
	if (d1->da_data > (d2->da_data + d2->da_data_len))
		return (1);
	return (0);
}

static void
sbc_overlap_store(disk_io_t *io)
{
	disk_params_t	*d	= io->da_params;
	avl_index_t	where	= 0;

	assert(d != NULL);

	(void) pthread_mutex_lock(&d->d_mutex);
	(void) avl_find(&d->d_mmap_overlaps, (void *)io, &where);
	avl_insert(&d->d_mmap_overlaps, (void *)io, where);
	(void) pthread_mutex_unlock(&d->d_mutex);
}

static void
sbc_overlap_free(disk_io_t *io)
{
	disk_params_t	*d = io->da_params;

	assert(d != NULL);

	(void) pthread_mutex_lock(&d->d_mutex);
	avl_remove(&d->d_mmap_overlaps, (void *)io);
	if (d->d_mmap_paused == True) {
		d->d_mmap_paused = False;
		(void) pthread_cond_signal(&d->d_mmap_cond);
	}
	(void) pthread_mutex_unlock(&d->d_mutex);
}

static void
sbc_overlap_check(disk_io_t *io)
{
	disk_params_t	*d = io->da_params;

	assert(d != NULL);
recheck:
	(void) pthread_mutex_lock(&d->d_mutex);
	if (avl_find(&d->d_mmap_overlaps, (void *)io, NULL) != NULL) {
		d->d_mmap_paused = True;
		while (d->d_mmap_paused == True)
			(void) pthread_cond_wait(&d->d_mmap_cond,
			    &d->d_mutex);

		/*
		 * After waiting on the condition variable the link
		 * list has changed because someone removed a command.
		 * So, drop the lock and reexamine the list.
		 */
		(void) pthread_mutex_unlock(&d->d_mutex);
		goto recheck;
	}
	(void) pthread_mutex_unlock(&d->d_mutex);
}

/*
 * []----
 * | sbc_overlap_flush -- wait until everyone has reported in
 * []----
 */
static void
sbc_overlap_flush(disk_params_t *d)
{
	assert(d != NULL);
recheck:
	(void) pthread_mutex_lock(&d->d_mutex);
	if (avl_numnodes(&d->d_mmap_overlaps) != 0) {
		d->d_mmap_paused = True;
		while (d->d_mmap_paused == True)
			(void) pthread_cond_wait(&d->d_mmap_cond,
			    &d->d_mutex);

		/*
		 * After waiting on the condition variable the link
		 * list has changed because someone removed a command.
		 * So, drop the lock and reexamine the list.
		 */
		(void) pthread_mutex_unlock(&d->d_mutex);
		goto recheck;
	}
	(void) pthread_mutex_unlock(&d->d_mutex);
}

/*
 * []----
 * | Command table for LBA emulation. This is at the end of the file because
 * | it's big and ugly. ;-) To make for fast translation to the appropriate
 * | emulation routine we just have a big command table with all 256 possible
 * | entries. Most will report STATUS_CHECK, unsupport operation. By doing
 * | this we can avoid error checking for command range.
 * []----
 */
static scsi_cmd_table_t lba_table[] = {
	/* 0x00 -- 0x0f */
	{ spc_tur,		NULL,	NULL,		"TEST_UNIT_READY" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_request_sense,	NULL,	NULL,		"REQUEST_SENSE" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_read, NULL, sbc_read_cmplt,		"READ" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_write, sbc_write_data, sbc_write_cmplt,	"WRITE" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x10 -- 0x1f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_inquiry, NULL, NULL,			"INQUIRY" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_mselect, spc_mselect_data, NULL,		"MODE_SELECT" },
	{ sbc_reserve,		NULL,	NULL,		"RESERVE" },
	{ sbc_release,		NULL,	NULL,		"RELEASE" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_msense,		NULL,	NULL,		"MODE_SENSE" },
	{ sbc_startstop,	NULL,	NULL,		"START_STOP" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_send_diag,	NULL,	NULL,		"SEND_DIAG" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x20 -- 0x2f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_recap,		NULL,	NULL,		"READ_CAPACITY" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_read, NULL, sbc_read_cmplt,		"READ_G1" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_write, sbc_write_data, sbc_write_cmplt,	"WRITE_G1" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x30 -- 0x3f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_synccache,	NULL,	NULL,		"SYNC_CACHE" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x40 -- 0x4f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	"LOG_SENSE" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x50 -- 0x5f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	"PERSISTENT_IN" },
	{ spc_unsupported,	NULL,	NULL,	"PERSISTENT_OUT" },

	/* 0x60 -- 0x6f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x70 -- 0x7f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x80 -- 0x8f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_read, NULL, sbc_read_cmplt,		"READ_G4" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_write, sbc_write_data, sbc_write_cmplt,	"WRITE_G4" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0x90 -- 0x9f */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ sbc_service_actiong4,	NULL,	NULL,		"SVC_ACTION_G4" },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xa0 - 0xaf */
	{ spc_report_luns,	NULL,	NULL,		"REPORT_LUNS" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_report_tpgs,	NULL,	NULL,		"REPORT_TPGS" },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xb0 -- 0xbf */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xc0 -- 0xcf */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xd0 -- 0xdf */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xe0 -- 0xef */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },

	/* 0xf0 -- 0xff */
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
	{ spc_unsupported,	NULL,	NULL,	NULL },
};
