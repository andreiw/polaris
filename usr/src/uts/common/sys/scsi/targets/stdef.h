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

#ifndef	_SYS_SCSI_TARGETS_STDEF_H
#define	_SYS_SCSI_TARGETS_STDEF_H

#pragma ident	"@(#)stdef.h	1.101	06/07/19 SMI"

#include <sys/sunddi.h>
#include <sys/note.h>
#include <sys/condvar.h>
#include <sys/kstat.h>
#include <sys/scsi/scsi_types.h>
#include <sys/mtio.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for SCSI tape drives.
 */

/*
 * Maximum variable length record size for a single request
 */
#define	ST_MAXRECSIZE_VARIABLE	65535

/*
 * If the requested record size exceeds ST_MAXRECSIZE_VARIABLE,
 * then the following define is used.
 */
#define	ST_MAXRECSIZE_VARIABLE_LIMIT	65534

#define	ST_MAXRECSIZE_FIXED	(63<<10)	/* maximum fixed record size */
#define	INF 1000000000

/*
 * Supported tape device types plus default type for opening.
 * Types 10 - 13, are special (ancient too) drives - *NOT SUPPORTED*
 * Types 14 - 1f, are 1/4-inch cartridge drives.
 * Types 20 - 28, are 1/2-inch cartridge or reel drives.
 * Types 28+, are rdat (vcr) drives.
 */
#define	ST_TYPE_INVALID		0x00

#define	ST_TYPE_SYSGEN1	MT_ISSYSGEN11	/* Sysgen with QIC-11 only */
#define	ST_TYPE_SYSGEN	MT_ISSYSGEN	/* Sysgen with QIC-24 and QIC-11 */

#define	ST_TYPE_DEFAULT	MT_ISDEFAULT	/* Generic 1/4" or undetermined  */
#define	ST_TYPE_EMULEX	MT_ISMT02	/* Emulex MT-02 */
#define	ST_TYPE_ARCHIVE	MT_ISVIPER1	/* Archive QIC-150 */
#define	ST_TYPE_WANGTEK	MT_ISWANGTEK1	/* Wangtek QIC-150 */

#define	ST_TYPE_CDC	MT_ISCDC	/* CDC - (not tested) */
#define	ST_TYPE_FUJI	MT_ISFUJI	/* Fujitsu - (not tested) */
#define	ST_TYPE_KENNEDY	MT_ISKENNEDY	/* Kennedy */
#define	ST_TYPE_ANRITSU	MT_ISANRITSU	/* Anritsu */
#define	ST_TYPE_HP	MT_ISHP		/* HP */
#define	ST_TYPE_HIC	MT_ISCCS23	/* Generic 1/2" Cartridge */
#define	ST_TYPE_REEL	MT_ISCCS24	/* Generic 1/2" Reel Tape */
#define	ST_TYPE_DAT	MT_ISCCS28	/* Generic DAT Tape */

#define	ST_TYPE_EXABYTE	MT_ISEXABYTE	/* Exabyte 8200 */
#define	ST_TYPE_EXB8500	MT_ISEXB8500	/* Exabyte 8500 */
#define	ST_TYPE_WANGTHS	MT_ISWANGTHS	/* Wangtek 6130HS */
#define	ST_TYPE_WANGDAT	MT_ISWANGDAT	/* WangDAT */
#define	ST_TYPE_PYTHON  MT_ISPYTHON	/* Archive Python DAT */
#define	ST_TYPE_STC3490 MT_ISSTC	/* IBM STC 3490 */
#define	ST_TYPE_TAND25G MT_ISTAND25G	/* TANDBERG 2.5G */
#define	ST_TYPE_DLT	MT_ISDLT	/* DLT */
#define	ST_TYPE_STK9840	MT_ISSTK9840	/* StorageTek 9840, 9940, 9840B */
#define	ST_TYPE_BMDLT1	MT_ISBMDLT1	/* Benchmark DTL1 */
#define	ST_TYPE_LTO	MT_LTO		/* sun: LTO's by HP, Seagate, IBM.. */
#define	ST_TYPE_AIT	MT_ISAIT	/* Sony AIT I, II, III and SAIT */
#define	ST_LAST_TYPE	ST_TYPE_AIT	/* Add new above type and change this */


/* Internal flags */
#define	ST_DYNAMIC		0x2000	/* Device name has been dynamically */
					/* alloc'ed from the st.conf entry, */
					/* instead of being used from the */
					/* st_drivetypes array. */

/*
 * Defines for supported drive options
 *
 * WARNING : THESE OPTIONS SHOULD NEVER BE CHANGED, AS OLDER CONFIGURATIONS
 * 		WILL DEPEND ON THE FLAG VALUES REMAINING THE SAME
 */
#define	ST_VARIABLE		0x001	/* Device supports variable	*/
					/* length record sizes		*/
#define	ST_QIC			0x002	/* QIC tape device 		*/
#define	ST_REEL			0x004	/* 1/2-inch reel tape device	*/
#define	ST_BSF			0x008	/* Device supports backspace	*/
					/* file as in mt(1) bsf : 	*/
					/* backspace over EOF marks.	*/
					/* Devices not supporting bsf 	*/
					/* will fail with ENOTTY upon	*/
					/* use of bsf			*/
#define	ST_BSR			0x010	/* Device supports backspace	*/
					/* record as in mt(1) bsr :	*/
					/* backspace over records. If	*/
					/* the device does not support 	*/
					/* bsr, the st driver emulates	*/
					/* the action by rewinding the	*/
					/* tape and using forward space	*/
					/* file (fsf) to the correct	*/
					/* file and then uses forward	*/
					/* space record (fsr) to the	*/
					/* correct  record		*/
#define	ST_LONG_ERASE		0x020	/* Device needs a longer time	*/
					/* than normal to erase		*/
#define	ST_AUTODEN_OVERRIDE	0x040	/* Auto-Density override flag	*/
					/* Device can figure out the	*/
					/* tape density automatically,	*/
					/* without issuing a		*/
					/* mode-select/mode-sense 	*/
#define	ST_NOBUF		0x080	/* Don't use buffered mode.	*/
					/* This disables the device's	*/
					/* ability for buffered	writes	*/
					/* I.e. The device acknowledges	*/
					/* write completion after the	*/
					/* data is written to the	*/
					/* device's buffer, but before	*/
					/* all the data is actually	*/
					/* written to tape		*/
#define	ST_RESERVED_BIT1	0x100	/* resreved bit 		*/
					/* parity while talking to it. 	*/
#define	ST_KNOWS_EOD		0x200	/* Device knows when EOD (End	*/
					/* of Data) has been reached.	*/
					/* If the device knows EOD, st	*/
					/* uses fast file skipping.	*/
					/* If it does not know EOD,	*/
					/* file skipping happens one	*/
					/* file at a time. 		*/
#define	ST_UNLOADABLE		0x400	/* Device will not complain if	*/
					/* the st driver is unloaded &	*/
					/* loaded again; e.g. will	*/
					/* return the correct inquiry	*/
					/* string			*/
#define	ST_SOFT_ERROR_REPORTING 0x800	/* Do request or log sense on	*/
					/* close to report soft errors.	*/
					/* Currently only Exabyte and	*/
					/* DAT drives support this	*/
					/* feature.  			*/
#define	ST_LONG_TIMEOUTS	0x1000	/* Device needs 5 times longer	*/
					/* timeouts for normal		*/
					/* operation			*/
#define	ST_BUFFERED_WRITES	0x4000	/* The data is buffered in the	*/
					/* driver and pre-acked to the	*/
					/* application 			*/
#define	ST_NO_RECSIZE_LIMIT	0x8000	/* For variable record size	*/
					/* devices only. If flag is	*/
					/* set, then don't limit	*/
					/* record size to 64k as in	*/
					/* pre-Solaris 2.4 releases.	*/
					/* The only limit on the	*/
					/* record size will be the max	*/
					/* record size the device can	*/
					/* handle or the max DMA	*/
					/* transfer size of the		*/
					/* machine, which ever is	*/
					/* smaller. Beware of		*/
					/* incompatabilities with	*/
					/* tapes of pre-Solaris 2.4	*/
					/* OS's written with large	*/
					/* (>64k) block sizes, as	*/
					/* their true block size is	*/
					/* a max of approx 64k		*/
#define	ST_MODE_SEL_COMP	0x10000	/* use mode select of device	*/
					/* configuration page (0x10) to */
					/* enable/disable compression	*/
					/* instead of density codes for */
					/* the "c" and "u" devices	*/
#define	ST_NO_RESERVE_RELEASE	0x20000	/* For devices which do not	*/
					/* support RESERVE/RELEASE SCSI	*/
					/* command. If this is enabled	*/
					/* then reserve/release would	*/
					/* not be used during open/	*/
					/* close for High Availability	*/
#define	ST_READ_IGNORE_ILI	0x40000 /* This flag is only applicable */
					/* to variable block devices 	*/
					/* which support the SILI bit	*/
					/* option. It indicates that 	*/
					/* the SILI bit will be ignored */
					/* during reads  		*/
#define	ST_READ_IGNORE_EOFS 	0x80000 /* When this flag is set two 	*/
					/* EOF marks do not indicate an */
					/* EOM. This option is only	*/
					/* supported on 1/2" reel tapes */
#define	ST_SHORT_FILEMARKS	0x100000 /* This option applies only to */
					/* EXABYTE 8mm tape drives 	*/
					/* which support short 		*/
					/* filemarks. When this flag 	*/
					/* is set, short filemarks 	*/
					/* will be used for writing	*/
					/* filemarks. 			*/
#define	ST_EJECT_ON_CHANGER_FAILURE 0x200000 /* When this flag is set   */
					/* and the tape is trapped in   */
					/* the medium changer, the tape */
					/* is automatically ejected	*/
#define	ST_RETRY_ON_RECOVERED_DEFERRED_ERROR 0x400000
					/* This option applies only to  */
					/* IBM MAGSTAR 3590. If this    */
					/* flag is set, the st driver   */
					/* will retry the last cmd if   */
					/* the last error cause a check */
					/* condition with error code    */
					/* 0x71 and sense code  0x01 	*/
#define	ST_KNOWS_MEDIA		0x800000 /* Use configured media type	*/
					/* detected to select correct   */
					/* density code.		*/
#define	ST_WORMABLE		0x1000000
					/* Drive is capable of doing	*/
					/* Write Appends only at EOM	*/
					/* if WORM media type is loaded */
#define	ST_CLN_TYPE_1		0x10000000 /* When this flag is set,	*/
					/* the tape drive provides the	*/
					/* clean bit information in	*/
					/* byte 21, bitmask 0x08 of	*/
					/* Request Sense data		*/
#define	ST_CLN_TYPE_2		0x20000000 /* When this flag is set,	*/
					/* the tape drive provides the	*/
					/* clean bit information in	*/
					/* byte 70, bitmask 0xc0 of	*/
					/* Request Sense data		*/
#define	ST_CLN_TYPE_3		0x40000000 /* When this flag is set,	*/
					/* the tape drive provides the	*/
					/* clean bit information in	*/
					/* byte 18, bitmask 0x01 of	*/
					/* Request Sense data		*/

#define	ST_CLN_MASK	(ST_CLN_TYPE_1 | ST_CLN_TYPE_2 | ST_CLN_TYPE_3)
#define	ST_VALID_OPTS	(ST_VARIABLE | ST_QIC | ST_REEL | ST_BSF | ST_BSR |\
	ST_LONG_ERASE | ST_AUTODEN_OVERRIDE | ST_NOBUF | ST_KNOWS_EOD |\
	ST_UNLOADABLE | ST_SOFT_ERROR_REPORTING | ST_LONG_TIMEOUTS |\
	ST_NO_RECSIZE_LIMIT | ST_MODE_SEL_COMP | ST_NO_RESERVE_RELEASE |\
	ST_READ_IGNORE_ILI | ST_READ_IGNORE_EOFS | ST_SHORT_FILEMARKS |\
	ST_EJECT_ON_CHANGER_FAILURE | ST_RETRY_ON_RECOVERED_DEFERRED_ERROR |\
	ST_WORMABLE | ST_CLN_TYPE_1 | ST_CLN_TYPE_2 | ST_CLN_TYPE_3)

#define	NDENSITIES	MT_NDENSITIES
#define	NSPEEDS		MT_NSPEEDS

/*
 * defines for Log Sense Pages
 */
#define	SUPPORTED_LOG_PAGES_PAGE	0x00
#define	TAPE_SEQUENTIAL_PAGE		0x0c
#define	TAPE_ALERT_PAGE			0x2e

/*
 * Log Page Control definitions
 */
#define	CURRENT_THRESHOLD_VALUES	0x00
#define	CURRENT_CUMULATIVE_VALUES	0x40
#define	DEFAULT_THRESHOLD_VALUES	0x80
#define	DEFAULT_CUMULATIVE_VALUES	0xC0

/*
 * Tape Alert Flag definitions
 */
typedef enum {
	TAF_READ_WARN			= 0x01,
	TAF_WRITE_WARN			= 0x02,
	TAF_HARD_ERR			= 0x03,
	TAF_MEDIA_ERR			= 0x04,
	TAF_READ_FAIL			= 0x05,
	TAF_WRITE_FAIL			= 0x06,
	TAF_MEDIA_LIFE			= 0x07,
	TAF_MEDIA_NOT_DATA_GRADE	= 0x08,
	TAF_WRITE_PROTECTED		= 0x09,
	TAF_NO_MEDIA_REMOVE		= 0x0A,
	TAF_CLEANING_MEDIA		= 0x0B,
	TAF_UNSUPPERTED_FORMAT		= 0x0C,
	TAF_RECOVERED_TAPE_BREAK	= 0x0D,
	TAF_TAPE_BREAK_FAUL		= 0x0E,
	TAF_CART_MEM_FAIL		= 0x0F,
	TAF_FORCED_EJECT		= 0x10,
	TAF_READ_ONLY_FORMAT		= 0x11,
	TAF_TAPE_DIR_CORRUPT		= 0x12,
	TAF_NEARING_MEDIA_LIFE		= 0x13,
	TAF_CLEAN_NOW			= 0x14,
	TAF_CLEAN_PERIODIC		= 0x15,
	TAF_EXP_CLEAN_CART		= 0x16,
	TAF_INVALID_CLEAN_MEDIA		= 0x17,
	TAF_RETENSION_REQUEST		= 0x18,
	TAF_DUAL_PORT_INTERFACE_ERR	= 0x19,
	TAF_COOLING_FAN_FAIL		= 0x1A,
	TAF_POWER_SUPPLY_FAIL		= 0x1B,
	TAF_POWER_CONSUMPTION		= 0x1C,
	TAF_DRIVE_MAINT_REQUEST		= 0x1D,
	TAF_HARDWARE_A			= 0x1E,
	TAF_HARDWARE_B			= 0x1F,
	TAF_INTERFACE			= 0x20,
	TAF_EJECT_MEDIA			= 0x21,
	TAF_DOWNLOAD_FAIL		= 0x22,
	TAF_DRIVE_HUMIDITY		= 0x23,
	TAF_DRIVE_TEMP			= 0x24,
	TAF_DRIVE_VOLTAGE		= 0x25,
	TAF_PREDICTIVE_FAIL		= 0x26,
	TAF_DIAG_REQUIRED		= 0x27,
	TAF_LOADER_HDWR_A		= 0x28,
	TAF_LOADER_STRAY_TAPE		= 0x29,
	TAF_LOADER_HDWR_B		= 0x2A,
	TAF_LOADER_DOOR			= 0x2B,
	TAF_LOADER_HDWR_C		= 0x2C,
	TAF_LOADER_MAGAZINE		= 0x2D,
	TAF_LOADER_PREDICTIVE_FAIL	= 0x2E,
	TAF_LOST_STATISTICS		= 0x32,
	TAF_TAPE_DIR_CURRUPT_UNLOAD	= 0x33,
	TAF_TAPE_SYS_WRT_FAIL		= 0x34,
	TAF_TAPE_SYS_RD_FAIL		= 0x35,
	TAF_NO_START_OF_DATA		= 0x36,
	TAF_WORM_INTEGRITY		= 0x3B,
	TAF_WORM_OVRWRT_ATTEMPT		= 0x3C
}tape_alert_flags;

/*
 * For ST_TYPE_STK9840 drives only. STK drive doesn't support retension
 * so they reuse TAF_RETENSION_REQUEST.
 */
#define	CLEAN_FOR_ERRORS		 0x18


#define	TAPE_ALERT_SUPPORT_UNKNOWN	0x00
#define	TAPE_ALERT_NOT_SUPPORTED	0x01
#define	TAPE_ALERT_SUPPORTED		0x02
#define	TAPE_ALERT_STILL_DIRTY		0x04
#define	TAPE_SEQUENTIAL_SUPPORTED	0x08
#define	TAPE_PREVIOUSLY_DIRTY		0x10

#define	TAPE_ALERT_MAX_PARA		64
#define	TAPE_SEQUENTIAL_PAGE_PARA	64	/* way more then really used */
#define	SEQUENTIAL_NEED_CLN		0x0100

/*
 * Parameters
 */
#define	ST_NAMESIZE	44	/* size of pretty string for vid/pid */
#define	VIDLEN		8	/* size of vendor identifier length */
#define	PIDLEN		16	/* size of product identifier length */
#define	VIDPIDLEN	(VIDLEN + PIDLEN)


struct st_drivetype {
	char	name[ST_NAMESIZE];	/* Name, for debug */
	char	length;			/* Length of vendor id */
	char	vid[VIDPIDLEN];		/* Vendor id and model (product) id */
	char	type;			/* Drive type for driver */
	int	bsize;			/* Block size */
	int	options;		/* Drive options */
	int	max_rretries;		/* Max read retries */
	int	max_wretries;		/* Max write retries */
	uchar_t	densities[NDENSITIES];	/* density codes, low->hi */
	uchar_t	default_density;	/* default density for this drive */
	uchar_t	mediatype[NDENSITIES];	/* was speed. mediatype for density. */
	ushort_t non_motion_timeout;	/* Inquiry type commands */
	ushort_t io_timeout;		/* I/O timeout in seconds */
	ushort_t rewind_timeout;	/* rewind timeout in seconds */
	ushort_t space_timeout;		/* space cmd timeout in seconds */
	ushort_t load_timeout;		/* load tape time in seconds */
	ushort_t unload_timeout;	/* unload tape time in seconds */
	ushort_t erase_timeout;		/* erase timeout. seconds */
};

#define	MINUTES(val)	((val) * 60)

struct comp_mode_page {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	:		6,
		dcc:		1,	/* Data Compression Capable */
		dce:		1;	/* Data Compression Enable */
	uchar_t	:		5,
		red:		2,	/* Report Exceptions on Decompress */
		dde:		1;	/* Data Decompression Enabled */
	uchar_t	comp_alg_msb;		/* Compression Algorithm */
	uchar_t comp_alg_high;
	uchar_t	comp_alg_low;
	uchar_t	comp_alg_lsb;
	uchar_t	decomp_alg_msb;		/* Decompression Algorithm */
	uchar_t decomp_alg_high;
	uchar_t	decomp_alg_low;
	uchar_t	decomp_alg_lsb;
	uchar_t	reservered0;
	uchar_t	reservered1;
	uchar_t	reservered2;
	uchar_t	reservered3;

#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	dce:		1,	/* Data Compression Enable */
		dcc:		1,	/* Data Compression Capable */
		:		6;
	uchar_t	dde:		1,	/* Data Decompression Enabled */
		red:		2,	/* Report Exceptions on Decompress */
		:		5;
	uchar_t	comp_alg_msb;		/* Compression Algorithm */
	uchar_t comp_alg_high;
	uchar_t	comp_alg_low;
	uchar_t	comp_alg_lsb;
	uchar_t	decomp_alg_msb;		/* Decompression Algorithm */
	uchar_t decomp_alg_high;
	uchar_t	decomp_alg_low;
	uchar_t	decomp_alg_lsb;
	uchar_t	reservered0;
	uchar_t	reservered1;
	uchar_t	reservered2;
	uchar_t	reservered3;
#endif
};

struct dev_mode_page {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	act_format:	5,	/* active format */
		caf:		1,	/* Change Active Format */
		cap:		1,	/* Change Active Partition OBSOLETE */
		:		1;
	uchar_t	act_partition;		/* active partition */
	uchar_t	wrt_buf_full_ratio;	/* write buffer full ratio */
	uchar_t	rd_buf_full_ratio;	/* read buffer full ratio */
	uchar_t	wrt_delay_time_msb;	/* write delay time MSB */
	uchar_t	wrt_delay_time_lsb;	/* write delay time LSB */
	uchar_t	rew:		1,	/* Report Early Warning */
		robo:		1,	/* Reverse Object Buffer Order */
		socf:		2,	/* Stop On Consecutive Filemarks */
		avc:		1,	/* Automatic Velocity Control */
		rsmk:		1,	/* Report SetMarKs OBSOLETE */
		lois:		1,	/* Logical Object Identifiers Support */
		obr:		1;	/* Object Buffer Recovery */
	uchar_t	gap_size;		/* OBSOLETE */
	uchar_t	bam:		1,	/* Block Address Mode */
		bmal:		1,	/* Block Address Mode Lock */
		swp:		1,	/* Software Write Protection */
		sew:		1,	/* Sync data after Early Warning */
		eeg:		1,	/* Enable Early Waring */
		eod_defined:	3;
	uchar_t	buf_size_leot_msb;	/* Buffer size after early warning */
	uchar_t	buf_size_leot_mid;
	uchar_t	buf_size_leot_lsb;
	uchar_t	comp_alg;		/* Compression Algorithm (enable) */
	uchar_t	prmwp:		1,	/* PeRManent Write Protect */
		perswp:		1,	/* persistant write protection */
		asocwp:		1,	/* associated write protect */
		rew_on_rst:	2,	/* rewind on reset */
		oir:		1,	/* Only If Reserved */
		wtre:		2;	/* Worm Tamper Read Enable */

#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	:		1,
		cap:		1,	/* Change Active Partition OBSOLETE */
		caf:		1,	/* Change Active Format */
		act_format:	5;	/* active format */
	uchar_t	act_partition;		/* active partition */
	uchar_t	wrt_buf_full_ratio;	/* write buffer full ratio */
	uchar_t	rd_buf_full_ratio;	/* read buffer full ratio */
	uchar_t	wrt_delay_time_msb;	/* write delay time MSB */
	uchar_t	wrt_delay_time_lsb;	/* write delay time LSB */
	uchar_t	obr:		1,	/* Object Buffer Recovery */
		lois:		1,	/* Logical Object Identifiers Support */
		rsmk:		1,	/* Report SetMarKs OBSOLETE */
		avc:		1,	/* Automatic Velocity Control */
		socf:		2,	/* Stop On Consecutive Filemarks */
		robo:		1,	/* Reverse Object Buffer Order */
		rew:		1;	/* Report Early Warning */
	uchar_t	gap_size;		/* OBSELETE */
	uchar_t	eod_defined:	3,
		eeg:		1,	/* Enable Early Waring */
		sew:		1,	/* Sync data after Early Warning */
		swp:		1,	/* Software Write Protection */
		bmal:		1,	/* Block Address Mode Lock */
		bam:		1;	/* Block Address Mode */
	uchar_t	buf_size_leot_msb;	/* Buffer size after early warning */
	uchar_t	buf_size_leot_mid;
	uchar_t	buf_size_leot_lsb;
	uchar_t	comp_alg;		/* Compression Algorithm (enable) */
	uchar_t	wtre:		2,	/* Worm Tamper Read Enable */
		oir:		1,	/* Only If Reserved */
		rew_on_rst:	2,	/* rewind on reset */
		asocwp:		1,	/* associated write protect */
		perswp:		1,	/* persistant write protection */
		prmwp:		1;	/* PeRManent Write Protect */
#endif
};

typedef union {
	struct comp_mode_page	comp;
	struct dev_mode_page	dev;
}modepage;

/*
 *
 * Parameter list for the MODE_SELECT and MODE_SENSE commands.
 * The parameter list contains a header, followed by zero or more
 * block descriptors, followed by vendor unique parameters, if any.
 *
 */
#define	MSIZE	0x0c		/* Size without additional pages */
struct seq_mode {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	data_len;	/* sense data length, sense only */
	uchar_t	media_type;	/* medium type, sense only */
	uchar_t	speed	:4,	/* speed */
		bufm	:3,	/* buffered mode */
		wp	:1;	/* write protected, sense only */
	uchar_t	bd_len;		/* block length in bytes */
	uchar_t	density;	/* density code */
	uchar_t	high_nb;	/* number of logical blocks on the medium */
	uchar_t	mid_nb;		/* that are to be formatted with the density */
	uchar_t	low_nb;		/* code and block length in block descriptor */
	uchar_t	reserved;	/* reserved */
	uchar_t	high_bl;	/* block length */
	uchar_t	mid_bl;		/*   "      "   */
	uchar_t	low_bl;		/*   "      "   */
	uchar_t page_code:	6,
		:		1,
		ps:		1; /* Page Savable sense only */
	uchar_t	page_len;
	modepage page;

#elif defined(_BIT_FIELDS_HTOL)
	uchar_t	data_len;	/* sense data length, sense only */
	uchar_t	media_type;	/* medium type, sense only */
	uchar_t	wp	:1,	/* write protected, sense only */
		bufm	:3,	/* buffered mode */
		speed	:4;	/* speed */
	uchar_t	bd_len;		/* block length in bytes */
	uchar_t	density;	/* density code */
	uchar_t	high_nb;	/* number of logical blocks on the medium */
	uchar_t	mid_nb;		/* that are to be formatted with the density */
	uchar_t	low_nb;		/* code and block length in block descriptor */
	uchar_t	reserved;	/* reserved */
	uchar_t	high_bl;	/* block length */
	uchar_t	mid_bl;		/*   "      "   */
	uchar_t	low_bl;		/*   "      "   */
	uchar_t	ps:		1, /* Page Savable sense only */
		:		1,
		page_code:	6;
	uchar_t	page_len;
	modepage page;
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
};

/*
 * Data returned from the READ BLOCK LIMITS command.
 */

#define	RBLSIZE	(sizeof (struct read_blklim))
struct read_blklim {
#if defined(_BIT_FIELDS_HTOL)
	uchar_t	reserved:	3;	/* reserved */
	uchar_t granularity:	5;	/* Minimum Modularity */
#elif defined(_BIT_FIELDS_LTOH)
	uchar_t granularity:	5;	/* Minimum Modularity */
	uchar_t	reserved:	3;	/* reserved */
#endif
	uchar_t	max_hi;			/* Maximum block length, high byte */
	uchar_t	max_mid;		/* Maximum block length, middle byte */
	uchar_t	max_lo;			/* Maximum block length, low byte */
	uchar_t	min_hi;			/* Minimum block length, high byte */
	uchar_t	min_lo;			/* Minimum block length, low byte */
};

#ifdef _KERNEL

#if defined(__i386) || defined(__amd64)
/* Data structure used in big block I/O on x86/x64 platform */

/*
 * alloc more than one contig_mem, so mutiple I/O can be
 * on-going simultaneously
 */
#define	ST_MAX_CONTIG_MEM_NUM	3

/*
 * 60K is used due to the limitation(size) of the intermediate buffer
 * in DMA bind code(rootnex.c), which is 64K. If the I/O buf is page
 * aligned, HBA can do 64K DMA, but if not, HBA can only do
 * 64K - PAGESIZE = 60K DMA due to the copy to/from intermediate
 * buffer will keep the page offset.
 */
#define	ST_BIGBLK_XFER		60 * 1024
struct contig_mem {
	struct contig_mem *cm_next;
	size_t cm_len;
	caddr_t cm_addr;
	ddi_acc_handle_t cm_acc_hdl;
	struct buf *cm_bp;
	int cm_use_sbuf;
};
#endif

#endif /* _KERNEL */

/*
 * eof/eot/eom codes.
 */

typedef enum {
	ST_NO_EOF,		/* 0x00 */
	ST_EOF_PENDING,		/* 0x01	filemark pending */
	ST_EOF,			/* 0x02	at filemark */
	ST_EOT_PENDING,		/* 0x03	logical eot pending */
	ST_EOT,			/* 0x04	at logical eot */
	ST_EOM,			/* 0x05	at physical eot */
	ST_WRITE_AFTER_EOM	/* 0x06	flag for allowing writes after EOM */
}media_st;

#define	IN_EOF(un)	(un->un_eof == ST_EOF_PENDING || un->un_eof == ST_EOF)

/*
 * operation codes
 */
typedef enum {
	ST_OP_NIL,	/* 0 */
	ST_OP_CTL,	/* 1 */
	ST_OP_READ,	/* 2 */
	ST_OP_WRITE,	/* 3 */
	ST_OP_WEOF	/* 4 */
}optype;

typedef enum { RDWR, RDONLY, WORM, RDWORM, ERROR  } writablity;

/*
 * Private info for scsi tapes. Pointed to by the un_private pointer
 * of one of the SCSI_DEVICE chains.
 */

struct scsi_tape {
	struct scsi_device *un_sd;	/* back pointer to SCSI_DEVICE */
	struct scsi_pkt *un_rqs;	/* ptr to request sense command */
	struct scsi_pkt *un_mkr_pkt;	/* ptr to marker packet */
	kcondvar_t un_sbuf_cv;		/* cv on ownership of special buf */
	kcondvar_t un_queue_cv;		/* cv on all queued commands */
	struct	buf *un_sbufp;		/* for use in special io */
	char	*un_srqbufp;		/* sense buffer for special io */
	kcondvar_t un_clscv;		/* closing cv */
	struct	buf *un_quef;		/* head of wait queue */
	struct	buf *un_quel;		/* tail of wait queue */
	struct	buf *un_runqf;		/* head of run queue */
	struct	buf *un_runql;		/* tail of run queue */
	struct seq_mode *un_mspl;	/* ptr to mode select info */
	struct st_drivetype *un_dp;	/* ptr to drive table entry */
	uint_t	un_dp_size;		/* size of un_dp alloc'ed */
	caddr_t	un_tmpbuf;		/* buf for append, autodens ops */
	daddr_t	un_blkno;		/* block # in file (512 byte blocks) */
	int	un_oflags;		/* open flags */
	int	un_fileno;		/* current file number on tape */
	int	un_err_fileno;		/* file where error occurred */
	daddr_t	un_err_blkno;		/* block in file where err occurred */
	uint_t	un_err_resid;		/* resid from last error */
	short	un_fmneeded;		/* filemarks to be written - HP only */
	dev_t	un_dev;			/* unix device */
	uchar_t	un_attached;		/* unit known && attached */
	int	un_pwr_mgmt;		/* power management state */
	uchar_t	un_density_known;	/* density is known */
	uchar_t	un_curdens;		/* index into density table */
	optype	un_lastop;		/* last I/O was: read/write/ctl */
	media_st un_eof;		/* eof states */
	uchar_t	un_laststate;		/* last state */
	uchar_t	un_state;		/* current state */
	uchar_t	un_status;		/* status from last sense */
	uchar_t	un_retry_ct;		/* retry count */
	uchar_t	un_tran_retry_ct;	/* transport retry count */
	writablity un_read_only;	/* RDWR, RDONLY, WORM, RDWORM */
	uchar_t	un_test_append;		/* check writing at end of tape */
	uchar_t un_arq_enabled;		/* auto request sense enabled */
	uchar_t un_untagged_qing;	/* hba has untagged quing */
	uchar_t	un_allow_large_xfer;	/* allow >64k xfers if requested */
	uchar_t	un_sbuf_busy;		/* sbuf busy flag */
	uchar_t	un_ncmds;		/* number of commands outstanding */
	uchar_t	un_throttle;		/* curr. max number of cmds outst. */
	uchar_t	un_last_throttle;	/* saved max number of cmds outst. */
	uchar_t	un_max_throttle;	/* max poss. number cmds outstanding */
	uchar_t	un_persistence;		/* 1 = persistence on, 0 off */
	uchar_t	un_persist_errors;	/* 1 = persistenced flagged */
	uchar_t	un_flush_on_errors;	/* HBA will flush all I/O's on a */
					/* check condidtion or error */
	uint_t	un_kbytes_xferred;	/* bytes (in K) counter */
	uint_t	un_last_resid;		/* keep last resid, for PE */
	uint_t	un_last_count;		/* keep last count, for PE */
	struct 	kstat *un_stats;	/* for I/O statistics */
	struct buf *un_rqs_bp;		/* bp used in rqpkt */
	struct	buf *un_wf;		/* head of write queue */
	struct	buf *un_wl;		/* tail of write queue */
	struct	read_blklim *un_rbl;	/* ptr to read block limit info */
	int	un_maxdma;		/* max dma xfer allowed by HBA */
	uint_t	un_bsize;		/* block size currently being used */
	int	un_maxbsize;		/* max block size allowed by drive */
	uint_t	un_minbsize;		/* min block size allowed by drive */
	int	un_errno;		/* errno (b_error) */
	kcondvar_t	un_state_cv;	/* mediastate condition variable */
	enum mtio_state	un_mediastate;	/* current media state */
	enum mtio_state	un_specified_mediastate;	/* expected state */
	timeout_id_t	un_delay_tid;	/* delayed cv tid */
	timeout_id_t	un_hib_tid;	/* handle interrupt busy tid */
	opaque_t	un_swr_token;	/* scsi_watch request token */
	uchar_t	un_comp_page;		/* compression page */
	uchar_t	un_rsvd_status;		/* Reservation Status */
	kstat_t *un_errstats;		/* for error statistics */
	int	un_init_options;  	/* Init time drive options */
	int	un_save_fileno;		/* Save here for recovery */
	daddr_t	un_save_blkno;		/* Save here for recovery */
	uchar_t	un_restore_pos;		/* Indication to do recovery */
	int	un_suspend_fileno;	/* Save fileno for SUSPEND */
	daddr_t	un_suspend_blkno;	/* Save blkno for SUSPEND */
	uchar_t	un_silent_skip;		/* to catch short reads */
	short	un_tids_at_suspend; /* timeouts set at suspend */
	kcondvar_t	un_tape_busy_cv;	/* busy cv */
	kcondvar_t	un_suspend_cv;	/* busy cv */
					/* restore on close */
	uchar_t	un_eject_tape_on_failure; /* 1 = eject tape, 0 = don't */
	uchar_t	un_HeadClean; 		/* support and need head cleaning? */
	uchar_t	un_rqs_state;		/* see define below */
	caddr_t	un_uscsi_rqs_buf;	/* uscsi_rqs: buffer for RQS data */
	uchar_t	un_data_mod;		/* Device required data mod */
	writablity (*un_wormable) (struct scsi_tape *un); /* worm test fuct */
	int un_max_cdb_sz;		/* max cdb size to use */

#if defined(__i386) || defined(__amd64)
	ddi_dma_handle_t un_contig_mem_hdl;
	struct contig_mem *un_contig_mem;
	int un_contig_mem_available_num;
	int un_contig_mem_total_num;
	size_t un_max_contig_mem_len;
	kcondvar_t un_contig_mem_cv;
#endif
};
/*
 * device error kstats
 */
struct st_errstats {
	struct kstat_named	st_softerrs;
	struct kstat_named	st_harderrs;
	struct kstat_named	st_transerrs;
	struct kstat_named	st_vid;
	struct kstat_named	st_pid;
	struct kstat_named	st_revision;
	struct kstat_named	st_serial;
};

/*
 * generic log page struct
 */
struct log_page {
#if defined(_BIT_FIELDS_LTOH)
	uchar_t	code	:6,	/* page code number */
			:2;	/* reserved */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t		:2,	/* reserved */
		code	:6;	/* page code number */
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	reserved;	/* reserved */
	uchar_t	length_hi;	/* length of bytes to follow (msb) */
	uchar_t	length_lo;	/* length of bytes to follow (lsb) */
	/*
	 * Log parameters follow right after this...
	 */
};

/*
 * generic log page parameter struct
 */
struct log_param {
	uchar_t	pc_hi;			/* parameter code (msb) */
	uchar_t	pc_lo;			/* parameter code (lsb) */
#if defined(_BIT_FIELDS_LTOH)
	uchar_t		lp	: 1,	/* list parameter */
				: 1,	/* reserved */
			tmc	: 2,	/* threshold met criteria */
			etc	: 1,	/* enable threshold comparison */
			tsd	: 1,	/* target save disable */
			ds	: 1,	/* disable save */
			du	: 1;	/* disable update */
#elif defined(_BIT_FIELDS_HTOL)
	uchar_t		du	: 1,	/* disable update */
			ds	: 1,	/* disable save */
			tsd	: 1,	/* target save disable */
			etc	: 1,	/* enable threshold comparison */
			tmc	: 2,	/* threshold met criteria */
				: 1,	/* reserved */
			lp	: 1;	/* list parameter */
#endif	/* _BIT_FIELDS_LTOH */
	uchar_t	length;		/* length of bytes to follow */
	/*
	 * Parameter values follow right after this...
	 */
};
/*
 * TapeAlert structures
 */

struct st_tape_alert_parameter {
	struct log_param log_param;
	uchar_t	param_value;
};

struct st_tape_alert {
	struct log_page log_page;
	struct st_tape_alert_parameter param[TAPE_ALERT_MAX_PARA];
};

#define	TAPE_ALERT_PARAMETER_LENGTH \
	(sizeof (struct st_tape_alert_parameter)) * TAPE_ALERT_MAX_PARA

struct log_sequential_page_parameter {
	struct log_param log_param;
	uchar_t param_value[8];
};

struct log_sequential_page {
	struct log_page log_page;
	struct log_sequential_page_parameter param[TAPE_SEQUENTIAL_PAGE_PARA];
};

#if !defined(__lint)
_NOTE(MUTEX_PROTECTS_DATA(scsi_device::sd_mutex, scsi_tape))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_tape::un_dp))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_tape::un_sd))
_NOTE(SCHEME_PROTECTS_DATA("not shared", scsi_tape::un_rqs))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_tape::un_bsize))
_NOTE(SCHEME_PROTECTS_DATA("not shared", scsi_arq_status))
_NOTE(SCHEME_PROTECTS_DATA("save sharing",
	scsi_tape::un_allow_large_xfer
	scsi_tape::un_maxbsize
	scsi_tape::un_maxdma
))
#endif


/*
 * driver states..
 */
#define	ST_STATE_CLOSED				0
#define	ST_STATE_OFFLINE			1
#define	ST_STATE_INITIALIZING			2
#define	ST_STATE_OPENING			3
#define	ST_STATE_OPEN_PENDING_IO		4
#define	ST_STATE_APPEND_TESTING			5
#define	ST_STATE_OPEN				6
#define	ST_STATE_RESOURCE_WAIT			7
#define	ST_STATE_CLOSING			8
#define	ST_STATE_SENSING			9
#define	ST_STATE_CLOSE_PENDING_OPEN		10

/*
 * Power management state
 */
#define	ST_PWR_NORMAL				0
#define	ST_PWR_SUSPENDED			1




/* un_rqs_state codes */

#define	ST_RQS_OVR		0x1	/* RQS data was overwritten */
#define	ST_RQS_VALID		0x2	/* RQS data is valid */
#define	ST_RQS_READ		0x4	/* RQS data was read */
#define	ST_RQS_ERROR		0x8	/* RQS resulted in an EIO */

/*
 * stintr codes
 */

#define	COMMAND_DONE					0
#define	COMMAND_DONE_ERROR				1
#define	COMMAND_DONE_ERROR_RECOVERED			2
#define	QUE_COMMAND					3
#define	QUE_BUSY_COMMAND				4
#define	QUE_SENSE					5
#define	JUST_RETURN					6
#define	COMMAND_DONE_EACCES				7
#define	QUE_LAST_COMMAND				8


/*
 *	Reservation Status
 *
 * ST_INIT_RESERVE      -Used to check if the reservation has been lost
 *		         in between opens and also to indicate the reservation
 *		         has not been done till now.
 * ST_RELEASE	        -Tape Unit is Released.
 * ST_RESERVE	        -Tape Unit is Reserved.
 * ST_PRESERVE_RESERVE  -Reservation is to be preserved across opens.
 *
 */
#define	ST_INIT_RESERVE			0x001
#define	ST_RELEASE			0x002
#define	ST_RESERVE			0x004
#define	ST_PRESERVE_RESERVE		0x008
#define	ST_RESERVATION_CONFLICT 	0x010
#define	ST_LOST_RESERVE			0x020
#define	ST_APPLICATION_RESERVATIONS	0x040
#define	ST_LOST_RESERVE_BETWEEN_OPENS  \
		(ST_RESERVE | ST_LOST_RESERVE | ST_PRESERVE_RESERVE)

/*
 * Service action defines for Persistant Reservation Commands
 */
#define	ST_SA_SCSI3_REGISTER			0x00
#define	ST_SA_SCSI3_RESERVE			0x01
#define	ST_SA_SCSI3_RELEASE			0x02
#define	ST_SA_SCSI3_CLEAR			0x03
#define	ST_SA_SCSI3_PREEMPT			0x04
#define	ST_SA_SCSI3_PREEMPTANDABORT		0x05
#define	ST_SA_SCSI3_REGISTERANDIGNOREKEY	0x06
#define	ST_SA_MASK				0x1f

#define	ST_RESERVATION_DELAY		500000

/*
 * Asynch I/O tunables
 */
#define	ST_MAX_THROTTLE		4

/*
 * 60 minutes seems a reasonable amount of time
 * to wait for tape space operations to complete.
 *
 */
#define	ST_SPACE_TIME	MINUTES(60)	/* 60 minutes per space operation */
#define	ST_LONG_SPACE_TIME_X	5	/* multipiler for long space ops */

/*
 * 2 minutes seems a reasonable amount of time
 * to wait for tape i/o operations to complete.
 *
 */
#define	ST_IO_TIME	MINUTES(2)	/* minutes per i/o */
#define	ST_LONG_TIMEOUT_X	5	/* multiplier for very long timeouts */


/*
 * 10 seconds is what we'll wait if we get a Busy Status back
 */
#define	ST_STATUS_BUSY_TIMEOUT	10*hz	/* seconds Busy Waiting */
#define	ST_TRAN_BUSY_TIMEOUT	1*hz	/* seconds retry on TRAN_BSY */
#define	ST_INTERRUPT_CONTEXT	1
#define	ST_START_CONTEXT	2

/*
 * Number of times we'll retry a normal operation.
 *
 * XXX This includes retries due to transport failure as well as
 * XXX busy timeouts- Need to distinguish between Target and Transport
 * XXX failure.
 */

#define	ST_RETRY_COUNT		20

/*
 * Number of times to retry a failed selection
 */
#define	ST_SEL_RETRY_COUNT		2

/*
 * es_code value for deferred error
 * should be moved to sense.h
 */

#define	ST_DEFERRED_ERROR		0x01

/*
 * Maximum number of units (determined by minor device byte)
 */
#define	ST_MAXUNIT	128

/*
 * Time to wait for completion of a command before cancelling it.
 * For SUSPEND use only
 */
#define	ST_WAIT_CMDS_COMPLETE		10	/* seconds */

#ifndef	SECSIZE
#define	SECSIZE	512
#endif
#ifndef	SECDIV
#define	SECDIV	9
#endif

/*
 * convenient defines
 */
#define	ST_SCSI_DEVP	(un->un_sd)
#define	ST_DEVINFO		(ST_SCSI_DEVP->sd_dev)
#define	ST_INQUIRY		(ST_SCSI_DEVP->sd_inq)
#define	ST_RQSENSE		(ST_SCSI_DEVP->sd_sense)
#define	ST_MUTEX		(&ST_SCSI_DEVP->sd_mutex)
#define	ROUTE			(&ST_SCSI_DEVP->sd_address)

#define	BSD_BEHAVIOR	(getminor(un->un_dev) & MT_BSD)
#define	SVR4_BEHAVIOR	((getminor(un->un_dev) & MT_BSD) == 0)
#define	SCBP(pkt)		((struct scsi_status *)(pkt)->pkt_scbp)
#define	SCBP_C(pkt)		((*(pkt)->pkt_scbp) & STATUS_MASK)
#define	CDBP(pkt)		((union scsi_cdb *)(pkt)->pkt_cdbp)
#define	BP_PKT(bp)		((struct scsi_pkt *)(bp)->av_back)
#define	SET_BP_PKT(bp, pkt)	((bp)->av_back = (struct buf *)(pkt))
#define	BP_UCMD(bp)		((struct uscsi_cmd *)(bp)->b_back)
#define	USCSI_CMD(bp)	(((bp) == un->un_sbufp) && (BP_UCMD(bp)))

#define	IS_CLOSING(un)	((un)->un_state == ST_STATE_CLOSING || \
	((un)->un_state == ST_STATE_SENSING && \
		(un)->un_laststate == ST_STATE_CLOSING))

#define	ASYNC_CMD	0
#define	SYNC_CMD	1

/*
 * Flush tape wait queue as needed.
 */

#define	IS_PE_FLAG_SET(un) ((un)->un_persistence && (un)->un_persist_errors)

#define	TURN_PE_ON(un)		st_turn_pe_on(un)
#define	TURN_PE_OFF(un)		st_turn_pe_off(un)
#define	SET_PE_FLAG(un)		st_set_pe_flag(un)
#define	CLEAR_PE(un)		st_clear_pe(un)

#define	st_bioerror(bp, error) \
		{ bioerror(bp, error); \
		un->un_errno = error; }

/*
 * Macros for internal coding of count for SPACE command:
 *
 * Isfmk is 1 when spacing filemarks; 0 when spacing records:
 * bit 24 set indicates a space filemark command.
 * Fmk sets the filemark bit (24) and changes a backspace
 * count into a positive number with the sign bit set.
 * Blk changes a backspace count into a positive number with
 * the sign bit set.
 * space_cnt converts backwards counts to negative numbers.
 */
#define	Isfmk(x)	((x & (1<<24)) != 0)
#define	Fmk(x)		((1<<24)|((x < 0) ? ((-(x)) | (1<<30)): x))
#define	Blk(x)		((x < 0)? ((-(x))|(1<<30)): x)
#define	space_cnt(x)	(((x) & (1<<30))? (-((x)&((1<<24)-1))):(x)&((1<<24)-1))


#define	GET_SOFT_STATE(dev)						\
	register struct scsi_tape *un;					\
	register int instance;						\
									\
	instance = MTUNIT(dev);						\
	if ((un = ddi_get_soft_state(st_state, instance)) == NULL)	\
		return (ENXIO);

/*
 * Debugging turned on via conditional compilation switch -DSTDEBUG
 */
#ifdef DEBUG
#define	STDEBUG
#endif

#ifdef	STDEBUG
#define	DEBUGGING	((scsi_options & SCSI_DEBUG_TGT) || st_debug > 1)


#define	ST_DEBUG1	if (st_debug >= 1) scsi_log	/* initialization */
#define	ST_DEBUG	ST_DEBUG1

#define	ST_DEBUG2	if (st_debug >= 2) scsi_log	/* errors and UA's */
#define	ST_DEBUG3	if (st_debug >= 3) scsi_log	/* func calls */
#define	ST_DEBUG4	if (st_debug >= 4) scsi_log	/* ioctl calls */
#define	ST_DEBUG5	if (st_debug >= 5) scsi_log
#define	ST_DEBUG6	if (st_debug >= 6) scsi_log	/* full data tracking */

#define	ST_DEBUG_SP	if (st_debug == 10) scsi_log	/* special cases */

#else

#define	st_debug	(0)
#define	DEBUGGING	(0)
#define	ST_DEBUG	if (0) scsi_log
#define	ST_DEBUG1	if (0) scsi_log
#define	ST_DEBUG2	if (0) scsi_log
#define	ST_DEBUG3	if (0) scsi_log
#define	ST_DEBUG4	if (0) scsi_log
#define	ST_DEBUG5	if (0) scsi_log
#define	ST_DEBUG6	if (0) scsi_log

#define	ST_DEBUG_SP	if (0) scsi_log /* special cases */

#endif

/*
 * Media access values
 */
#define	MEDIA_ACCESS_DELAY 5000000	/* usecs wait for media state change */

/*
 * SCSI tape mode sense page information
 */
#define	ST_DEV_CONFIG_PAGE	0x10	/* device config mode page */
#define	ST_DEV_CONFIG_NO_COMP	0x00	/* use no compression */
#define	ST_DEV_CONFIG_DEF_COMP	0x01	/* use default compression alg */
#define	ST_COMPRESSION_DENSITY	3	/* compression minor number */

/*
 * SCSI tape data compression Page definition.
 */
#define	ST_DEV_DATACOMP_PAGE	0x0F	/* data compression page */



/*
 * maxbsize values
 */
#define	MAXBSIZE_UNKNOWN	-2	/*  not found yet */

#define	ONE_MEG			(1024 * 1024)

/*
 * generic soft error reporting
 *
 * What we are doing here is allowing a greater number of errors to occur on
 * smaller transfers (i.e. usually at the beginning of the tape), than on
 * the rest of the tape.
 *
 * A small transfer is defined as :
 * Transfers <= SOFT_ERROR_WARNING_THRESHOLD  allow about 1.5 times more errors
 *
 * A larget tranfer is defined as :
 * Transfers >  SOFT_ERROR_WARNING_THRESHOLD  allow normal amount
 *
 */
#define	READ_SOFT_ERROR_WARNING_THRESHOLD    (25 * ONE_MEG)
#define	WRITE_SOFT_ERROR_WARNING_THRESHOLD    (20 * ONE_MEG)

/*
 * soft error reporting for exabyte
 */
#define	TAPE_SENSE_LENGTH	32	/* allows for softerror info */

#define	SENSE_19_BITS  \
	"\20\10PF\07BPE\06FPE\05ME\04ECO\03TME\02TNP\01LBOT"
#define	SENSE_20_BITS  \
	"\20\10RSVD\07RSVD\06WP\05FMKE\04URE\03WE1\02SSE\01FW"
#define	SENSE_21_BITS  \
	"\20\10RSVD\07RSVD\06RRR\05CLND\04CLN\03PEOT\02WSEB\01WSE0"

/* these are defined in percentages */
#define	EXABYTE_WRITE_ERROR_THRESHOLD	6
#define	EXABYTE_READ_ERROR_THRESHOLD	3
/*
 * minumum amount of data transfer(MB) for checking soft error rate.
 */
#define	EXABYTE_MIN_TRANSFER			(25 * ONE_MEG)

#define	CLN	0x8
#define	CLND	0x10

/*
 * soft error reporting for Archive 4mm DAT
 */

#define	LOG_SENSE_LENGTH		0xff
#define	MIN_LOG_SENSE_LENGTH		0x2b
#define	DAT_SMALL_WRITE_ERROR_THRESHOLD	40	/* retries per 20 mg */
#define	DAT_LARGE_WRITE_ERROR_THRESHOLD	200	/* retries for more 20 mg */
#define	DAT_SMALL_READ_ERROR_THRESHOLD	5	/* errors allowed */
#define	DAT_LARGE_READ_ERROR_THRESHOLD	3	/* errors allowed */

/*
 * ST timeouts that need to be cancelled for suspend
 */
#define	ST_HIB_TID	0x01
#define	ST_DELAY_TID	0x02

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_TARGETS_STDEF_H */
