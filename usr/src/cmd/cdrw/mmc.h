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

#ifndef	_MMC_H
#define	_MMC_H

#pragma ident	"@(#)mmc.h	1.10	06/01/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* bytelengths for some SCSI data structures */
#define	SENSE_DATA_SIZE		16
#define	TRACK_INFO_SIZE		36
#define	DISC_INFO_BLOCK_SIZE	32
#define	INQUIRY_DATA_LENGTH	96
#define	GET_PERF_DATA_LEN	24
#define	SET_STREAM_DATA_LEN	28

#define	DEFAULT_SCSI_TIMEOUT    60

#define	MMC_FTR_HDR_LEN		8	/* byte len of Feature Header */
/*
 * byte length of the static part of a Feature Descriptor
 */
#define	MMC_FTR_DSCRPTR_BASE_LEN	4
#define	MMC_PRFL_DSCRPTR_LEN	4	/* byte len of Profile Descriptor */
/*
 * MMC Features; can be added to over time
 */
#define	MMC_FTR_PRFL_LIST	0x0000	/* Profile List Feature */
#define	MMC_FTR_CORE		0x0001	/* Core Feature */
#define	MMC_FTR_MORPHING	0x0002	/* Morphing Feature */
#define	MMC_FTR_REM_MED		0x0003	/* Removable Medium Feature */
#define	MMC_FTR_WR_PROTECT	0x0004	/* Write Protect Feature */
#define	MMC_FTR_RAND_READ	0x0010	/* Random Readable Feature */
#define	MMC_FTR_MULTI_READ	0x001D	/* Multi-Read Feature */
#define	MMC_FTR_CD_READ		0x001E	/* CD Read Feature */
#define	MMC_FTR_DVD_READ	0x001F	/* DVD Read Feature */
#define	MMC_FTR_RAND_WR		0x0020	/* Random Writable Feature */
#define	MMC_FTR_INC_STR_WR	0x0021	/* Incremental Streaming Writable */
#define	MMC_FTR_SCTR_ERSBL	0x0022	/* Sector Erasable Feature */
#define	MMC_FTR_FORMATTABLE	0x0023	/* Formattable Feature */
#define	MMC_FTR_DFCT_MNGMNT	0x0024	/* Hardware Defect Management Feature */
#define	MMC_FTR_RT_STREAM	0x0107	/* Real Time Streaming Feature */

int test_unit_ready(int fd);
int inquiry(int fd, uchar_t *inq);
int read_capacity(int fd, uchar_t *capbuf);
int read_track_info(int fd, int trackno, uchar_t *ti);
int mode_sense(int fd, uchar_t pc, int dbd, int page_len, uchar_t *buffer);
int mode_select(int fd, int page_len, uchar_t *buffer);
int read_toc(int fd, int format, int trackno, int buflen, uchar_t *buf);
int read_disc_info(int fd, uchar_t *di);
int get_configuration(int fd, uint16_t feature, int bufsize, uchar_t *buf);
int read10(int fd, uint32_t start_blk, uint16_t nblk, uchar_t *buf,
    uint32_t bufsize);
int write10(int fd, uint32_t start_blk, uint16_t nblk, uchar_t *buf,
    uint32_t bufsize);
int close_track(int fd, int trackno, int close_session, int immediate);
int blank_disc(int fd, int type, int immediate);
int read_cd(int fd, uint32_t start_blk, uint16_t nblk, uchar_t sector_type,
    uchar_t *buf, uint32_t bufsize);
int load_unload(int fd, int load);
int prevent_allow_mr(int fd, int op);
int read_header(int fd, uint32_t lba, uchar_t *buf);
int set_cd_speed(int fd, uint16_t read_speed, uint16_t write_speed);
int get_performance(int fd, int get_write_performance, uchar_t *perf);
int set_streaming(int fd, uchar_t *buf);
int rezero_unit(int fd);
int start_stop(int fd, int start);
int flush_cache(int fd);
int set_reservation(int fd, ulong_t size);
int format_media(int fd);
uint32_t read_format_capacity(int fd, uint_t *bsize);

int uscsi_error;		/* used for debugging failed uscsi */

#define	REZERO_UNIT_CMD 	0x01
#define	FORMAT_UNIT_CMD		0x04
#define	INQUIRY_CMD		0x12
#define	MODE_SELECT_6_CMD	0x15
#define	MODE_SENSE_6_CMD	0x1A
#define	START_STOP_CMD		0x1B
#define	PREVENT_ALLOW_CMD	0x1E
#define	READ_FORMAT_CAP_CMD	0x23
#define	READ_CAP_CMD		0x25
#define	READ_10_CMD		0x28
#define	WRITE_10_CMD		0x2A
#define	SYNC_CACHE_CMD		0x35
#define	READ_TOC_CMD		0x43
#define	MODE_SELECT_10_CMD	0x55
#define	MODE_SENSE_10_CMD	0x5A
#define	READ_HDR_CMD		0x44
#define	GET_CONFIG_CMD		0x46

#define	READ_INFO_CMD		0x51
#define	READ_TRACK_CMD		0x52
#define	SET_RESERVATION_CMD	0x53
#define	CLOSE_TRACK_CMD		0x5B

#define	BLANK_CMD		0xA1
#define	GET_PERFORMANCE_CMD	0xAC
#define	READ_DVD_STRUCTURE	0xAD
#define	READ_CD_CMD		0xBE
#define	SET_CD_SPEED		0xBB

#define	STREAM_CMD		0xB6
#define	READ_AUDIO_CMD		0xD8

#ifdef	__cplusplus
}
#endif

#endif /* _MMC_H */
