/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Use is subject to license terms.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 * unmodified.
 * 2. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following NO
 * ''WARRANTY'' disclaimer below (''Disclaimer''), without
 * modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 * disclaimer similar to the Disclaimer below and any redistribution
 * must be conditioned upon including a substantially similar
 * Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote
 * product derived from this software without specific prior written
 * permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 */

#ifndef _ATH_HAL_H
#define	_ATH_HAL_H

#pragma ident	"@(#)ath_hal.h	1.1	05/11/28 SMI"

/*
 * ath_hal.h is released by Atheros and used to describe the Atheros
 * Hardware Access Layer(HAL) interface. All kinds of data structures,
 * constant definition, APIs declaration are defined here.Clients of
 * the HAL call ath_hal_attach() to obtain a reference to an ath_hal
 * structure for use with the device. Hardware-related operations that
 * follow must call back into the HAL through interface, supplying the
 * reference as the first parameter.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* HAL version of this release */
#define	HAL_ABI_VERSION	0x04112900	/* YYMMDDnn */

/* HAL data type definition */
typedef void *		HAL_SOFTC;
typedef int		HAL_BUS_TAG;
typedef void *		HAL_BUS_HANDLE;
typedef uint32_t 	HAL_BUS_ADDR;
typedef uint16_t 	HAL_CTRY_CODE;	/* country code */
typedef uint16_t 	HAL_REG_DOMAIN;	/* regulatory domain code */

#define	HAL_NUM_TX_QUEUES	10		/* max number of tx queues */

#define	HAL_BEACON_PERIOD	0x0000ffff	/* beacon interval period */
#define	HAL_BEACON_ENA		0x00800000	/* beacon xmit enable */
#define	HAL_BEACON_RESET_TSF	0x01000000	/* clear TSF */

#define	CHANNEL_RAD_INT	0x0001	/* Radar interference detected on channel */
#define	CHANNEL_CW_INT	0x0002	/* CW interference detected on channel */
#define	CHANNEL_BUSY	0x0004	/* Busy, occupied or overlap with adjoin chan */
#define	CHANNEL_TURBO	0x0010	/* Turbo Channel */
#define	CHANNEL_CCK	0x0020	/* CCK channel */
#define	CHANNEL_OFDM	0x0040	/* OFDM channel */
#define	CHANNEL_2GHZ	0x0080	/* 2 GHz spectrum channel. */
#define	CHANNEL_5GHZ	0x0100	/* 5 GHz spectrum channel */
#define	CHANNEL_PASSIVE	0x0200	/* Only passive scan allowed in the channel */
#define	CHANNEL_DYN	0x0400	/* dynamic CCK-OFDM channel */
#define	CHANNEL_XR	0x0800	/* XR channel */
#define	CHANNEL_AR	0x8000  /* Software use: radar detected */

#define	CHANNEL_A	(CHANNEL_5GHZ|CHANNEL_OFDM)
#define	CHANNEL_B	(CHANNEL_2GHZ|CHANNEL_CCK)
#define	CHANNEL_PUREG	(CHANNEL_2GHZ|CHANNEL_OFDM)
#define	CHANNEL_G	(CHANNEL_2GHZ|CHANNEL_OFDM)
#define	CHANNEL_T	(CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define	CHANNEL_108G	(CHANNEL_2GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define	CHANNEL_X	(CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_XR)
#define	CHANNEL_ALL \
	(CHANNEL_OFDM|CHANNEL_CCK|CHANNEL_5GHZ|CHANNEL_2GHZ|CHANNEL_TURBO)
#define	CHANNEL_ALL_NOTURBO 	(CHANNEL_ALL &~ CHANNEL_TURBO)

#define	HAL_RSSI_EP_MULTIPLIER	(1<<7)	/* pow2 to optimize out * and / */

/* flags passed to tx descriptor setup methods */
#define	HAL_TXDESC_CLRDMASK	0x0001	/* clear destination filter mask */
#define	HAL_TXDESC_NOACK	0x0002	/* don't wait for ACK */
#define	HAL_TXDESC_RTSENA	0x0004	/* enable RTS */
#define	HAL_TXDESC_CTSENA	0x0008	/* enable CTS */
#define	HAL_TXDESC_INTREQ	0x0010	/* enable per-descriptor interrupt */
#define	HAL_TXDESC_VEOL		0x0020	/* mark virtual EOL */

/* flags passed to rx descriptor setup methods */
#define	HAL_RXDESC_INTREQ	0x0020	/* enable per-descriptor interrupt */

/* tx error flags */
#define	HAL_TXERR_XRETRY	0x01	/* excessive retries */
#define	HAL_TXERR_FILT		0x02	/* blocked by tx filtering */
#define	HAL_TXERR_FIFO		0x04	/* fifo underrun */
#define	HAL_TXSTAT_ALTRATE	0x80	/* alternate xmit rate used */

/* rx error flags */
#define	HAL_RXERR_CRC		0x01	/* CRC error on frame */
#define	HAL_RXERR_PHY		0x02	/* PHY error, rs_phyerr is valid */
#define	HAL_RXERR_FIFO		0x04	/* fifo overrun */
#define	HAL_RXERR_DECRYPT	0x08	/* non-Michael decrypt error */
#define	HAL_RXERR_MIC		0x10	/* Michael MIC decrypt error */

/* value found in rs_keyix to mark invalid entries */
#define	HAL_RXKEYIX_INVALID	((uint8_t)-1)

/* value used to specify no encryption key for xmit */
#define	HAL_TXKEYIX_INVALID	((uint32_t)-1)

/*
 * Status codes that may be returned by the HAL.  Note that
 * interfaces that return a status code set it only when an
 * error occurs--i.e. you cannot check it for success.
 */
typedef enum {
	HAL_OK		= 0,	/* No error */
	HAL_ENXIO	= 1,	/* No hardware present */
	HAL_ENOMEM	= 2,	/* Memory allocation failed */
	HAL_EIO		= 3,	/* Hardware didn't respond as expected */
	HAL_EEMAGIC	= 4,	/* EEPROM magic number invalid */
	HAL_EEVERSION	= 5,	/* EEPROM version invalid */
	HAL_EELOCKED	= 6,	/* EEPROM unreadable */
	HAL_EEBADSUM	= 7,	/* EEPROM checksum invalid */
	HAL_EEREAD	= 8,	/* EEPROM read problem */
	HAL_EEBADMAC	= 9,	/* EEPROM mac address invalid */
	HAL_EESIZE	= 10,	/* EEPROM size not supported */
	HAL_EEWRITE	= 11,	/* Attempt to change write-locked EEPROM */
	HAL_EINVAL	= 12,	/* Invalid parameter to function */
	HAL_ENOTSUPP	= 13,	/* Hardware revision not supported */
	HAL_ESELFTEST	= 14,	/* Hardware self-test failed */
	HAL_EINPROGRESS	= 15	/* Operation incomplete */
} HAL_STATUS;

typedef enum {
	AH_FALSE = 0,		/* NB: lots of code assumes false is zero */
	AH_TRUE  = 1
} HAL_BOOL;

typedef enum {
	HAL_CAP_REG_DMN		= 0,	/* current regulatory domain */
	HAL_CAP_CIPHER		= 1,	/* hardware supports cipher */
	HAL_CAP_TKIP_MIC	= 2,	/* handle TKIP MIC in hardware */
	HAL_CAP_TKIP_SPLIT	= 3,	/* hardware TKIP uses split keys */
	HAL_CAP_PHYCOUNTERS	= 4,	/* hardware PHY error counters */
	HAL_CAP_DIVERSITY	= 5,	/* hardware supports fast diversity */
	HAL_CAP_KEYCACHE_SIZE	= 6,	/* number of entries in key cache */
	HAL_CAP_NUM_TXQUEUES	= 7,	/* number of hardware xmit queues */
	HAL_CAP_VEOL		= 9,	/* hardware supports virtual EOL */
	HAL_CAP_PSPOLL		= 10,	/* hardware has working PS-Poll */
					/* support */
	HAL_CAP_DIAG		= 11,	/* hardware diagnostic support */
	HAL_CAP_COMPRESSION	= 12,	/* hardware supports compression */
	HAL_CAP_BURST		= 13,	/* hardware supports packet bursting */
	HAL_CAP_FASTFRAME	= 14,	/* hardware supoprts fast frames */
	HAL_CAP_TXPOW		= 15,	/* global tx power limit  */
	HAL_CAP_TPC		= 16	/* per-packet tx power control  */
} HAL_CAPABILITY_TYPE;

/*
 * "States" for setting the LED.  These correspond to
 * the possible 802.11 operational states and there may
 * be a many-to-one mapping between these states and the
 * actual hardware states for the LED's (i.e. the hardware
 * may have fewer states).
 */
typedef enum {
	HAL_LED_INIT	= 0,
	HAL_LED_SCAN	= 1,
	HAL_LED_AUTH	= 2,
	HAL_LED_ASSOC	= 3,
	HAL_LED_RUN	= 4
} HAL_LED_STATE;

/*
 * Transmit queue types/numbers.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
	HAL_TX_QUEUE_INACTIVE	= 0,	/* queue is inactive/unused */
	HAL_TX_QUEUE_DATA	= 1,	/* data xmit q's */
	HAL_TX_QUEUE_BEACON	= 2,	/* beacon xmit q */
	HAL_TX_QUEUE_CAB	= 3,	/* "crap after beacon" xmit q */
	HAL_TX_QUEUE_PSPOLL	= 4	/* power-save poll xmit q */
} HAL_TX_QUEUE;


/*
 * Transmit queue subtype.  These map directly to
 * WME Access Categories (except for UPSD).  Refer
 * to Table 5 of the WME spec.
 */
typedef enum {
	HAL_WME_AC_BK	= 0,		/* background access category */
	HAL_WME_AC_BE	= 1, 		/* best effort access category */
	HAL_WME_AC_VI	= 2,		/* video access category */
	HAL_WME_AC_VO	= 3,		/* voice access category */
	HAL_WME_UPSD	= 4		/* uplink power save */
} HAL_TX_QUEUE_SUBTYPE;

/*
 * Transmit queue flags that control various
 * operational parameters.
 */
typedef enum {
	TXQ_FLAG_TXOKINT_ENABLE	    = 0x0001,    /* enable TXOK interrupt */
	TXQ_FLAG_TXERRINT_ENABLE    = 0x0001,    /* enable TXERR interrupt */
	TXQ_FLAG_TXDESCINT_ENABLE   = 0x0002,    /* enable TXDESC interrupt */
	TXQ_FLAG_TXEOLINT_ENABLE    = 0x0004,    /* enable TXEOL interrupt */
	TXQ_FLAG_TXURNINT_ENABLE    = 0x0008,    /* enable TXURN interrupt */
	TXQ_FLAG_BACKOFF_DISABLE    = 0x0010,    /* disable Post Backoff  */
	TXQ_FLAG_COMPRESSION_ENABLE = 0x0020,    /* compression enabled */
	/* enable ready time expiry policy */
	TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE = 0x0040,
	/* enable backoff while	sending fragment burst */
	TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE = 0x0080
} HAL_TX_QUEUE_FLAGS;

typedef struct {
	uint32_t	tqi_ver;		/* hal TXQ version */
	HAL_TX_QUEUE_SUBTYPE tqi_subtype;	/* subtype if applicable */
	HAL_TX_QUEUE_FLAGS tqi_qflags;		/* flags (see above) */
	uint32_t	tqi_priority;		/* (not used) */
	uint32_t	tqi_aifs;		/* AIFS shift */
	int32_t		tqi_cwmin;		/* cwMin shift */
	int32_t		tqi_cwmax;		/* cwMax shift */
	uint16_t	tqi_shretry;		/* rts retry limit */
	uint16_t	tqi_lgretry;		/* long retry limit(not used) */
	uint32_t	tqi_cbrPeriod;
	uint32_t	tqi_cbrOverflowLimit;
	uint32_t	tqi_burstTime;
	uint32_t	tqi_readyTime;
} HAL_TXQ_INFO;

/* token to use for aifs, cwmin, cwmax */
#define	HAL_TXQ_USEDEFAULT	((uint32_t)-1)

/*
 * Transmit packet types.  This belongs in ah_desc.h, but
 * is here so we can give a proper type to various parameters
 * (and not require everyone include the file).
 *
 * NB: These values are intentionally assigned for
 *     direct use when setting up h/w descriptors.
 */
typedef enum {
	HAL_PKT_TYPE_NORMAL	= 0,
	HAL_PKT_TYPE_ATIM	= 1,
	HAL_PKT_TYPE_PSPOLL	= 2,
	HAL_PKT_TYPE_BEACON	= 3,
	HAL_PKT_TYPE_PROBE_RESP	= 4
} HAL_PKT_TYPE;

/* Rx Filter Frame Types */
typedef enum {
	HAL_RX_FILTER_UCAST	= 0x00000001,	/* Allow unicast frames */
	HAL_RX_FILTER_MCAST	= 0x00000002,	/* Allow multicast frames */
	HAL_RX_FILTER_BCAST	= 0x00000004,	/* Allow broadcast frames */
	HAL_RX_FILTER_CONTROL	= 0x00000008,	/* Allow control frames */
	HAL_RX_FILTER_BEACON	= 0x00000010,	/* Allow beacon frames */
	HAL_RX_FILTER_PROM	= 0x00000020,	/* Promiscuous mode */
	HAL_RX_FILTER_PROBEREQ	= 0x00000080,	/* Allow probe request frames */
	HAL_RX_FILTER_PHYERR	= 0x00000100,	/* Allow phy errors */
	HAL_RX_FILTER_PHYRADAR	= 0x00000200	/* Allow phy radar errors */
} HAL_RX_FILTER;

typedef enum {
	HAL_PM_UNDEFINED	= 0,
	HAL_PM_AUTO		= 1,
	HAL_PM_AWAKE		= 2,
	HAL_PM_FULL_SLEEP	= 3,
	HAL_PM_NETWORK_SLEEP	= 4
} HAL_POWER_MODE;

/*
 * NOTE WELL:
 * These are mapped to take advantage of the common locations for many of
 * the bits on all of the currently supported MAC chips. This is to make
 * the ISR as efficient as possible, while still abstracting HW differences.
 * When new hardware breaks this commonality this enumerated type, as well
 * as the HAL functions using it, must be modified. All values are directly
 * mapped unless commented otherwise.
 */
typedef enum {
	HAL_INT_RX	= 0x00000001,	/* Non-common mapping */
	HAL_INT_RXDESC	= 0x00000002,
	HAL_INT_RXNOFRM	= 0x00000008,
	HAL_INT_RXEOL	= 0x00000010,
	HAL_INT_RXORN	= 0x00000020,
	HAL_INT_TX	= 0x00000040,	/* Non-common mapping */
	HAL_INT_TXDESC	= 0x00000080,
	HAL_INT_TXURN	= 0x00000800,
	HAL_INT_MIB	= 0x00001000,
	HAL_INT_RXPHY	= 0x00004000,
	HAL_INT_RXKCM	= 0x00008000,
	HAL_INT_SWBA	= 0x00010000,
	HAL_INT_BMISS	= 0x00040000,
	HAL_INT_BNR	= 0x00100000,	/* Non-common mapping */
	HAL_INT_GPIO	= 0x01000000,
	HAL_INT_FATAL	= 0x40000000,	/* Non-common mapping */
	HAL_INT_GLOBAL	= INT_MIN,	/* Set/clear IER */

	/* Interrupt bits that map directly to ISR/IMR bits */
	HAL_INT_COMMON  = HAL_INT_RXNOFRM
			| HAL_INT_RXDESC
			| HAL_INT_RXEOL
			| HAL_INT_RXORN
			| HAL_INT_TXURN
			| HAL_INT_TXDESC
			| HAL_INT_MIB
			| HAL_INT_RXPHY
			| HAL_INT_RXKCM
			| HAL_INT_SWBA
			| HAL_INT_BMISS
			| HAL_INT_GPIO,
	HAL_INT_NOCARD	= -1	/* To signal the card was removed */
} HAL_INT;

typedef enum {
	HAL_RFGAIN_INACTIVE		= 0,
	HAL_RFGAIN_READ_REQUESTED	= 1,
	HAL_RFGAIN_NEED_CHANGE		= 2
} HAL_RFGAIN;

typedef enum {
	HAL_PHYERR_UNDERRUN		= 0,	/* Transmit underrun */
	HAL_PHYERR_TIMING		= 1,	/* Timing error */
	HAL_PHYERR_PARITY		= 2,	/* Illegal parity */
	HAL_PHYERR_RATE			= 3,	/* Illegal rate */
	HAL_PHYERR_LENGTH		= 4,	/* Illegal length */
	HAL_PHYERR_RADAR		= 5,	/* Radar detect */
	HAL_PHYERR_SERVICE		= 6,	/* Illegal service */
	HAL_PHYERR_TOR			= 7,	/* Transmit override receive */
	/* NB: these are specific to the 5212 */
	HAL_PHYERR_OFDM_TIMING		= 17,	/* */
	HAL_PHYERR_OFDM_SIGNAL_PARITY	= 18,	/* */
	HAL_PHYERR_OFDM_RATE_ILLEGAL	= 19,	/* */
	HAL_PHYERR_OFDM_LENGTH_ILLEGAL	= 20,	/* */
	HAL_PHYERR_OFDM_POWER_DROP	= 21,	/* */
	HAL_PHYERR_OFDM_SERVICE		= 22,	/* */
	HAL_PHYERR_OFDM_RESTART		= 23,	/* */
	HAL_PHYERR_CCK_TIMING		= 25,	/* */
	HAL_PHYERR_CCK_HEADER_CRC	= 26,	/* */
	HAL_PHYERR_CCK_RATE_ILLEGAL	= 27,	/* */
	HAL_PHYERR_CCK_SERVICE		= 30,	/* */
	HAL_PHYERR_CCK_RESTART		= 31	/* */
}HAL_PHYERR;

/*
 * Channels are specified by frequency.
 */
typedef struct {
	uint16_t	channel;	/* setting in Mhz */
	uint16_t	channelFlags;	/* see below */
} HAL_CHANNEL;


typedef struct {
	uint32_t	ackrcv_bad;
	uint32_t	rts_bad;
	uint32_t	rts_good;
	uint32_t	fcs_bad;
	uint32_t	beacons;
} HAL_MIB_STATS;


enum {
	CTRY_DEBUG	= 0x1ff,		/* debug country code */
	CTRY_DEFAULT	= 0			/* default country code */
};

enum {
	HAL_MODE_11A	= 0x001,
	HAL_MODE_TURBO	= 0x002,
	HAL_MODE_11B	= 0x004,
	HAL_MODE_PUREG	= 0x008,
	HAL_MODE_11G	= 0x008,
	HAL_MODE_108G	= 0x020,
	HAL_MODE_ALL	= 0xfff
};

typedef struct {
	int		rateCount;		/* NB: for proper padding */
	uint8_t	rateCodeToIndex[32];	/* back mapping */
	struct {
		uint8_t	valid;		/* valid for rate control use */
		uint8_t	phy;		/* CCK/OFDM/XR */
		uint16_t rateKbps;	/* transfer rate in kbs */
		uint8_t	rateCode;	/* rate for h/w descriptors */
		/* mask for enabling short preamble in CCK rate code */
		uint8_t	shortPreamble;
		/* value for supported rates info element of MLME */
		uint8_t	dot11Rate;
		/* index of next lower basic rate; used for dur. calcs */
		uint8_t	controlRate;
		uint16_t lpAckDuration;	/* long preamble ACK duration */
		uint16_t spAckDuration;	/* short preamble ACK duration */
	} info[32];
} HAL_RATE_TABLE;

typedef struct {
	uint32_t	rs_count;	/* number of valid entries */
	uint8_t	rs_rates[32];		/* rates */
} HAL_RATE_SET;

typedef enum {
	HAL_ANT_VARIABLE = 0,		/* variable by programming */
	HAL_ANT_FIXED_A	 = 1,		/* fixed to 11a frequencies */
	HAL_ANT_FIXED_B	 = 2		/* fixed to 11b frequencies */
} HAL_ANT_SETTING;

typedef enum {
	HAL_M_STA	= 1,		/* infrastructure station */
	HAL_M_IBSS	= 0,		/* IBSS (adhoc) station */
	HAL_M_HOSTAP	= 6,		/* Software Access Point */
	HAL_M_MONITOR	= 8		/* Monitor mode */
} HAL_OPMODE;

typedef struct {
	uint8_t	kv_type;		/* one of HAL_CIPHER */
	uint8_t	kv_pad;
	uint16_t	kv_len;		/* length in bits */
	uint8_t	kv_val[16];		/* enough for 128-bit keys */
	uint8_t	kv_mic[8];		/* TKIP MIC key */
} HAL_KEYVAL;

typedef enum {
	HAL_CIPHER_WEP		= 0,
	HAL_CIPHER_AES_OCB	= 1,
	HAL_CIPHER_AES_CCM	= 2,
	HAL_CIPHER_CKIP		= 3,
	HAL_CIPHER_TKIP		= 4,
	HAL_CIPHER_CLR		= 5,	/* no encryption */

	HAL_CIPHER_MIC		= 127	/* TKIP-MIC, not a cipher */
} HAL_CIPHER;

enum {
	HAL_SLOT_TIME_9	 = 9,
	HAL_SLOT_TIME_20 = 20
};

/*
 * Per-station beacon timer state.  Note that the specified
 * beacon interval (given in TU's) can also include flags
 * to force a TSF reset and to enable the beacon xmit logic.
 * If bs_cfpmaxduration is non-zero the hardware is setup to
 * coexist with a PCF-capable AP.
 */
typedef struct {
	uint32_t	bs_nexttbtt;		/* next beacon in TU */
	uint32_t	bs_nextdtim;		/* next DTIM in TU */
	uint32_t	bs_intval;		/* beacon interval+flags */
	uint32_t	bs_dtimperiod;
	uint16_t	bs_cfpperiod;		/* CFP period in TU */
	uint16_t	bs_cfpmaxduration;	/* max CFP duration in TU */
	uint32_t	bs_cfpnext;		/* next CFP in TU */
	uint16_t	bs_timoffset;		/* byte offset to TIM bitmap */
	uint16_t	bs_bmissthreshold;	/* beacon miss threshold */
	uint32_t	bs_sleepduration;	/* max sleep duration */
} HAL_BEACON_STATE;

/*
 * Per-node statistics maintained by the driver for use in
 * optimizing signal quality and other operational aspects.
 */
typedef struct {
	uint32_t	ns_avgbrssi;	/* average beacon rssi */
	uint32_t	ns_avgrssi;	/* average data rssi */
	uint32_t	ns_avgtxrssi;	/* average tx rssi */
} HAL_NODE_STATS;

/*
 * Transmit descriptor status.  This structure is filled
 * in only after the tx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * Note that ts_antenna may not be valid for all h/w.  It
 * should be used only if non-zero.
 */
struct ath_tx_status {
	uint16_t	ts_seqnum;	/* h/w assigned sequence number */
	uint16_t	ts_tstamp;	/* h/w assigned timestamp */
	uint8_t		ts_status;	/* frame status, 0 => xmit ok */
	uint8_t		ts_rate;	/* h/w transmit rate index */
	int8_t		ts_rssi;	/* tx ack RSSI */
	uint8_t		ts_shortretry;	/* # short retries */
	uint8_t		ts_longretry;	/* # long retries */
	uint8_t		ts_virtcol;	/* virtual collision count */
	uint8_t		ts_antenna;	/* antenna information */
};


/*
 * Receive descriptor status.  This structure is filled
 * in only after the rx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * If rx_status is zero, then the frame was received ok;
 * otherwise the error information is indicated and rs_phyerr
 * contains a phy error code if HAL_RXERR_PHY is set.  In general
 * the frame contents is undefined when an error occurred thought
 * for some errors (e.g. a decryption error), it may be meaningful.
 *
 * Note that the receive timestamp is expanded using the TSF to
 * a full 16 bits (regardless of what the h/w provides directly).
 *
 * rx_rssi is in units of dbm above the noise floor.  This value
 * is measured during the preamble and PLCP; i.e. with the initial
 * 4us of detection.  The noise floor is typically a consistent
 * -96dBm absolute power in a 20MHz channel.
 */
struct ath_rx_status {
	uint16_t	rs_datalen;	/* rx frame length */
	uint16_t	rs_tstamp;	/* h/w assigned timestamp */
	uint8_t		rs_status;	/* rx status, 0 => recv ok */
	uint8_t		rs_phyerr;	/* phy error code */
	int8_t		rs_rssi;	/* rx frame RSSI */
	uint8_t		rs_keyix;	/* key cache index */
	uint8_t		rs_rate;	/* h/w receive rate index */
	uint8_t		rs_antenna;	/* antenna information */
	uint8_t		rs_more;	/* see HAL_RXERR_XXX definition */
};

/*
 * Definitions for the software frame/packet descriptors used by
 * the Atheros HAL.  This definition obscures hardware-specific
 * details from the driver.  Drivers are expected to fillin the
 * portions of a descriptor that are not opaque then use HAL calls
 * to complete the work.  Status for completed frames is returned
 * in a device-independent format.
 */
#pragma pack(1)
struct ath_desc {
	/*
	 * The following definitions are passed directly
	 * the hardware and managed by the HAL.  Drivers
	 * should not touch those elements marked opaque.
	 */
	uint32_t	ds_link;	/* phys address of next descriptor */
	uint32_t	ds_data;	/* phys address of data buffer */
	uint32_t	ds_ctl0;	/* opaque DMA control 0 */
	uint32_t	ds_ctl1;	/* opaque DMA control 1 */
	uint32_t	ds_hw[4];	/* opaque h/w region */
	/*
	 * The remaining definitions are managed by software;
	 * these are valid only after the rx/tx process descriptor
	 * methods return a non-EINPROGRESS  code.
	 */
	union {
		struct ath_tx_status tx; /* xmit status */
		struct ath_rx_status rx; /* recv status */
	} ds_us;
};
#pragma pack()

#define	ds_txstat	ds_us.tx
#define	ds_rxstat	ds_us.rx

/*
 * Hardware Access Layer (HAL) API.
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an
 * ath_hal structure for use with the device.  Hardware-related operations
 * that follow must call back into the HAL through interface, supplying
 * the reference as the first parameter.  Note that before using the
 * reference returned by ath_hal_attach the caller should verify the
 * ABI version number.
 */
struct ath_hal {
	uint32_t	ah_magic;	/* consistency check magic number */
	uint32_t	ah_abi;		/* HAL ABI version */
	uint16_t	ah_devid;	/* PCI device ID */
	uint16_t	ah_subvendorid;	/* PCI subvendor ID */
	HAL_SOFTC	ah_sc;		/* back pointer to driver/os state */
	HAL_BUS_TAG	ah_st;		/* params for register r+w */
	HAL_BUS_HANDLE	ah_sh;
	HAL_CTRY_CODE	ah_countryCode;

	uint32_t	ah_macVersion;	/* MAC version id */
	uint16_t	ah_macRev;	/* MAC revision */
	uint16_t	ah_phyRev;	/* PHY revision */
	uint16_t	ah_analog5GhzRev; /* 2GHz radio revision */
	uint16_t	ah_analog2GhzRev; /* 5GHz radio revision */

	const HAL_RATE_TABLE *(*ah_getRateTable)(struct ath_hal *,
				uint32_t mode);
	void	  (*ah_detach) (struct ath_hal *);

	/* Reset functions */
	HAL_BOOL  (*ah_reset) (struct ath_hal *, HAL_OPMODE,
				HAL_CHANNEL *, HAL_BOOL bChannelChange,
				HAL_STATUS *status);
	HAL_BOOL   (*ah_phyDisable) (struct ath_hal *);
	void	   (*ah_setPCUConfig) (struct ath_hal *);
	HAL_BOOL  (*ah_perCalibration) (struct ath_hal *, HAL_CHANNEL *);
	HAL_BOOL  (*ah_setTxPowerLimit)(struct ath_hal *, uint32_t);

	/* Transmit functions */
	HAL_BOOL  (*ah_updateTxTrigLevel) (struct ath_hal *,
				HAL_BOOL incTrigLevel);
	int	  (*ah_setupTxQueue) (struct ath_hal *, HAL_TX_QUEUE,
				const HAL_TXQ_INFO *qInfo);
	HAL_BOOL  (*ah_setTxQueueProps) (struct ath_hal *, int q,
				const HAL_TXQ_INFO *qInfo);
	HAL_BOOL  (*ah_getTxQueueProps)(struct ath_hal *, int q,
				HAL_TXQ_INFO *qInfo);
	HAL_BOOL  (*ah_releaseTxQueue) (struct ath_hal *ah, uint32_t q);
	HAL_BOOL  (*ah_resetTxQueue) (struct ath_hal *ah, uint32_t q);
	uint32_t (*ah_getTxDP) (struct ath_hal *, uint32_t);
	HAL_BOOL  (*ah_setTxDP) (struct ath_hal *, uint32_t, uint32_t txdp);
	uint32_t (*ah_numTxPending)(struct ath_hal *, uint32_t q);
	HAL_BOOL  (*ah_startTxDma) (struct ath_hal *, uint32_t);
	HAL_BOOL  (*ah_stopTxDma) (struct ath_hal *, uint32_t);
	HAL_BOOL  (*ah_updateCTSForBursting)(struct ath_hal *,
				struct ath_desc *, struct ath_desc *,
				struct ath_desc *, struct ath_desc *,
				uint32_t, uint32_t);
	HAL_BOOL  (*ah_setupTxDesc) (struct ath_hal *, struct ath_desc *,
				uint32_t pktLen, uint32_t hdrLen,
				HAL_PKT_TYPE type, uint32_t txPower,
				uint32_t txRate0, uint32_t txTries0,
				uint32_t keyIx, uint32_t antMode,
				uint32_t flags, uint32_t rtsctsRate,
				uint32_t rtsctsDuration);
	HAL_BOOL  (*ah_setupXTxDesc) (struct ath_hal *, struct ath_desc *,
				uint32_t txRate1, uint32_t txTries1,
				uint32_t txRate2, uint32_t txTries2,
				uint32_t txRate3, uint32_t txTries3);
	HAL_BOOL  (*ah_fillTxDesc) (struct ath_hal *, struct ath_desc *,
				uint32_t segLen, HAL_BOOL firstSeg,
				HAL_BOOL lastSeg, const struct ath_desc *);
	HAL_STATUS (*ah_procTxDesc) (struct ath_hal *, struct ath_desc *);
	void	   (*ah_getTxIntrQueue)(struct ath_hal *, uint32_t *);

	/* Receive Functions */
	uint32_t (*ah_getRxDP) (struct ath_hal *);
	void	  (*ah_setRxDP) (struct ath_hal *, uint32_t rxdp);
	void	  (*ah_enableReceive) (struct ath_hal *);
	HAL_BOOL  (*ah_stopDmaReceive) (struct ath_hal *);
	void	  (*ah_startPcuReceive) (struct ath_hal *);
	void	  (*ah_stopPcuReceive) (struct ath_hal *);
	void	  (*ah_setMulticastFilter) (struct ath_hal *,
				uint32_t filter0, uint32_t filter1);
	HAL_BOOL  (*ah_setMulticastFilterIndex) (struct ath_hal *,
				uint32_t index);
	HAL_BOOL  (*ah_clrMulticastFilterIndex) (struct ath_hal *,
				uint32_t index);
	uint32_t (*ah_getRxFilter) (struct ath_hal *);
	void	  (*ah_setRxFilter) (struct ath_hal *, uint32_t);
	HAL_BOOL  (*ah_setupRxDesc) (struct ath_hal *, struct ath_desc *,
				uint32_t size, uint32_t flags);
	HAL_STATUS (*ah_procRxDesc) (struct ath_hal *, struct ath_desc *,
				uint32_t phyAddr, struct ath_desc *next);
	void	  (*ah_rxMonitor) (struct ath_hal *,
				const HAL_NODE_STATS *);
	void	  (*ah_procMibEvent) (struct ath_hal *,
				const HAL_NODE_STATS *);

	/* Misc Functions */
	HAL_STATUS  (*ah_getCapability) (struct ath_hal *,
				HAL_CAPABILITY_TYPE, uint32_t capability,
				uint32_t *result);
	HAL_BOOL    (*ah_setCapability) (struct ath_hal *,
				HAL_CAPABILITY_TYPE, uint32_t capability,
				uint32_t setting, HAL_STATUS *);
	HAL_BOOL    (*ah_getDiagState) (struct ath_hal *, int request,
				const void *args, uint32_t argsize,
				void **result, uint32_t *resultsize);
	void	  (*ah_getMacAddress) (struct ath_hal *, uint8_t *);
	HAL_BOOL  (*ah_setMacAddress) (struct ath_hal *, const uint8_t *);
	HAL_BOOL  (*ah_setRegulatoryDomain) (struct ath_hal *,
				uint16_t, HAL_STATUS *);
	void	  (*ah_setLedState) (struct ath_hal *, HAL_LED_STATE);
	void	  (*ah_writeAssocid) (struct ath_hal *,
				const uint8_t *bssid, uint16_t assocId);
	HAL_BOOL  (*ah_gpioCfgOutput) (struct ath_hal *, uint32_t gpio);
	HAL_BOOL  (*ah_gpioCfgInput) (struct ath_hal *, uint32_t gpio);
	uint32_t (*ah_gpioGet) (struct ath_hal *, uint32_t gpio);
	HAL_BOOL  (*ah_gpioSet) (struct ath_hal *,
				uint32_t gpio, uint32_t val);
	void	  (*ah_gpioSetIntr) (struct ath_hal *, uint32_t, uint32_t);
	uint32_t (*ah_getTsf32) (struct ath_hal *);
	uint64_t (*ah_getTsf64) (struct ath_hal *);
	void	  (*ah_resetTsf) (struct ath_hal *);
	HAL_BOOL  (*ah_detectCardPresent) (struct ath_hal *);
	void	  (*ah_updateMibCounters) (struct ath_hal *, HAL_MIB_STATS *);
	HAL_RFGAIN (*ah_getRfGain) (struct ath_hal *);
	uint32_t  (*ah_getDefAntenna) (struct ath_hal *);
	void	  (*ah_setDefAntenna) (struct ath_hal *, uint32_t);
	HAL_BOOL  (*ah_setSlotTime) (struct ath_hal *, uint32_t);
	uint32_t  (*ah_getSlotTime) (struct ath_hal *);
	HAL_BOOL  (*ah_setAckTimeout) (struct ath_hal *, uint32_t);
	uint32_t  (*ah_getAckTimeout) (struct ath_hal *);
	HAL_BOOL  (*ah_setCTSTimeout) (struct ath_hal *, uint32_t);
	uint32_t  (*ah_getCTSTimeout) (struct ath_hal *);

	/* Key Cache Functions */
	uint32_t (*ah_getKeyCacheSize) (struct ath_hal *);
	HAL_BOOL  (*ah_resetKeyCacheEntry) (struct ath_hal *, uint16_t);
	HAL_BOOL  (*ah_isKeyCacheEntryValid) (struct ath_hal *, uint16_t);
	HAL_BOOL  (*ah_setKeyCacheEntry) (struct ath_hal *,
				uint16_t, const HAL_KEYVAL *,
				const uint8_t *, int);
	HAL_BOOL  (*ah_setKeyCacheEntryMac) (struct ath_hal *,
				uint16_t, const uint8_t *);

	/* Power Management Functions */
	HAL_BOOL  (*ah_setPowerMode) (struct ath_hal *,
				HAL_POWER_MODE mode, int setChip,
				uint16_t sleepDuration);
	HAL_POWER_MODE (*ah_getPowerMode) (struct ath_hal *);
	HAL_BOOL  (*ah_initPSPoll) (struct ath_hal *);
	HAL_BOOL  (*ah_enablePSPoll) (struct ath_hal *,
				uint8_t *, uint16_t);
	HAL_BOOL  (*ah_disablePSPoll) (struct ath_hal *);

	/* Beacon Management Functions */
	void	  (*ah_beaconInit) (struct ath_hal *,
				uint32_t nexttbtt, uint32_t intval);
	void	  (*ah_setStationBeaconTimers) (struct ath_hal *,
				const HAL_BEACON_STATE *);
	void	  (*ah_resetStationBeaconTimers) (struct ath_hal *);
	HAL_BOOL  (*ah_waitForBeaconDone) (struct ath_hal *,
				HAL_BUS_ADDR);

	/* Interrupt functions */
	HAL_BOOL  (*ah_isInterruptPending) (struct ath_hal *);
	HAL_BOOL  (*ah_getPendingInterrupts) (struct ath_hal *, HAL_INT *);
	HAL_INT	  (*ah_getInterrupts) (struct ath_hal *);
	HAL_INT	  (*ah_setInterrupts) (struct ath_hal *, HAL_INT);
};

/*
 * Check the PCI vendor ID and device ID against Atheros' values
 * and return a printable description for any Atheros hardware.
 * AH_NULL is returned if the ID's do not describe Atheros hardware.
 */
extern	const char *ath_hal_probe(uint16_t vendorid, uint16_t devid);

/*
 * Attach the HAL for use with the specified device.  The device is
 * defined by the PCI device ID.  The caller provides an opaque pointer
 * to an upper-layer data structure (HAL_SOFTC) that is stored in the
 * HAL state block for later use.  Hardware register accesses are done
 * using the specified bus tag and handle.  On successful return a
 * reference to a state block is returned that must be supplied in all
 * subsequent HAL calls.  Storage associated with this reference is
 * dynamically allocated and must be freed by calling the ah_detach
 * method when the client is done.  If the attach operation fails a
 * null (AH_NULL) reference will be returned and a status code will
 * be returned if the status parameter is non-zero.
 */
extern	struct ath_hal *ath_hal_attach(uint16_t devid, HAL_SOFTC,
		HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_STATUS *status);

/*
 * Return a list of channels available for use with the hardware.
 * The list is based on what the hardware is capable of, the specified
 * country code, the modeSelect mask, and whether or not outdoor
 * channels are to be permitted.
 *
 * The channel list is returned in the supplied array.  maxchans
 * defines the maximum size of this array.  nchans contains the actual
 * number of channels returned.  If a problem occurred or there were
 * no channels that met the criteria then AH_FALSE is returned.
 */
extern	HAL_BOOL  ath_hal_init_channels(struct ath_hal *,
		HAL_CHANNEL *chans, uint32_t maxchans, uint32_t *nchans,
		HAL_CTRY_CODE cc, uint16_t modeSelect,
		HAL_BOOL enableOutdoor, HAL_BOOL enableExtendedChannels);

/*
 * Return bit mask of wireless modes supported by the hardware.
 */
extern	uint32_t  ath_hal_getwirelessmodes(struct ath_hal *, HAL_CTRY_CODE);

/*
 * Return rate table for specified mode (11a, 11b, 11g, etc).
 */
extern	const HAL_RATE_TABLE *  ath_hal_getratetable(struct ath_hal *,
		uint32_t mode);

/*
 * Calculate the transmit duration of a frame.
 */
extern uint16_t  ath_hal_computetxtime(struct ath_hal *,
		const HAL_RATE_TABLE *rates, uint32_t frameLen,
		uint16_t rateix, HAL_BOOL shortPreamble);

/*
 * Convert between IEEE channel number and channel frequency
 * using the specified channel flags; e.g. CHANNEL_2GHZ.
 */
extern	uint32_t  ath_hal_mhz2ieee(uint32_t mhz, uint32_t flags);
extern	uint32_t  ath_hal_ieee2mhz(uint32_t ieee, uint32_t flags);

/*
 * Return a version string for the HAL release.
 */
extern	char ath_hal_version[];

/*
 * Return a NULL-terminated array of build/configuration options.
 */
extern	const char *ath_hal_buildopts[];

/*
 * Macros to encapsulated HAL functions.
 */
#define	ATH_HAL_RESET(_ah, _opmode, _chan, _outdoor, _pstatus) \
	((*(_ah)->ah_reset)((_ah), (_opmode), (_chan), (_outdoor), (_pstatus)))
#define	ATH_HAL_GETCAPABILITY(_ah, _cap, _param, _result) \
	((*(_ah)->ah_getCapability)((_ah), (_cap), (_param), (_result)))
#define	ATH_HAL_GETREGDOMAIN(_ah, _prd) \
	ATH_HAL_GETCAPABILITY(_ah, HAL_CAP_REG_DMN, 0, (_prd))
#define	ATH_HAL_GETCOUNTRYCODE(_ah, _pcc) \
	(*(_pcc) = (_ah)->ah_countryCode)
#define	ATH_HAL_GETRATETABLE(_ah, _mode) \
	((*(_ah)->ah_getRateTable)((_ah), (_mode)))
#define	ATH_HAL_GETMAC(_ah, _mac) \
	((*(_ah)->ah_getMacAddress)((_ah), (_mac)))
#define	ATH_HAL_SETMAC(_ah, _mac) \
	((*(_ah)->ah_setMacAddress)((_ah), (_mac)))
#define	ATH_HAL_INTRSET(_ah, _mask) \
	((*(_ah)->ah_setInterrupts)((_ah), (_mask)))
#define	ATH_HAL_INTRGET(_ah) \
	((*(_ah)->ah_getInterrupts)((_ah)))
#define	ATH_HAL_INTRPEND(_ah) \
	((*(_ah)->ah_isInterruptPending)((_ah)))
#define	ATH_HAL_GETISR(_ah, _pmask) \
	((*(_ah)->ah_getPendingInterrupts)((_ah), (_pmask)))
#define	ATH_HAL_UPDATETXTRIGLEVEL(_ah, _inc) \
	((*(_ah)->ah_updateTxTrigLevel)((_ah), (_inc)))
#define	ATH_HAL_SETPOWER(_ah, _mode, _sleepduration) \
	((*(_ah)->ah_setPowerMode)((_ah), (_mode), AH_TRUE, (_sleepduration)))
#define	ATH_HAL_KEYRESET(_ah, _ix) \
	((*(_ah)->ah_resetKeyCacheEntry)((_ah), (_ix)))
#define	ATH_HAL_KEYSET(_ah, _ix, _pk) \
	((*(_ah)->ah_setKeyCacheEntry)((_ah), (_ix), (_pk), NULL, AH_FALSE))
#define	ATH_HAL_KEYISVALID(_ah, _ix) \
	(((*(_ah)->ah_isKeyCacheEntryValid)((_ah), (_ix))))
#define	ATH_HAL_KEYSETMAC(_ah, _ix, _mac) \
	((*(_ah)->ah_setKeyCacheEntryMac)((_ah), (_ix), (_mac)))
#define	ATH_HAL_KEYCACHESIZE(_ah) \
	((*(_ah)->ah_getKeyCacheSize)((_ah)))
#define	ATH_HAL_GETRXFILTER(_ah) \
	((*(_ah)->ah_getRxFilter)((_ah)))
#define	ATH_HAL_SETRXFILTER(_ah, _filter) \
	((*(_ah)->ah_setRxFilter)((_ah), (_filter)))
#define	ATH_HAL_SETMCASTFILTER(_ah, _mfilt0, _mfilt1) \
	((*(_ah)->ah_setMulticastFilter)((_ah), (_mfilt0), (_mfilt1)))
#define	ATH_HAL_WAITFORBEACON(_ah, _bf) \
	((*(_ah)->ah_waitForBeaconDone)((_ah), (_bf)->bf_daddr))
#define	ATH_HAL_PUTRXBUF(_ah, _bufaddr) \
	((*(_ah)->ah_setRxDP)((_ah), (_bufaddr)))
#define	ATH_HAL_GETTSF32(_ah) \
	((*(_ah)->ah_getTsf32)((_ah)))
#define	ATH_HAL_GETTSF64(_ah) \
	((*(_ah)->ah_getTsf64)((_ah)))
#define	ATH_HAL_RESETTSF(_ah) \
	((*(_ah)->ah_resetTsf)((_ah)))
#define	ATH_HAL_RXENA(_ah) \
	((*(_ah)->ah_enableReceive)((_ah)))
#define	ATH_HAL_PUTTXBUF(_ah, _q, _bufaddr) \
	((*(_ah)->ah_setTxDP)((_ah), (_q), (_bufaddr)))
#define	ATH_HAL_GETTXBUF(_ah, _q) \
	((*(_ah)->ah_getTxDP)((_ah), (_q)))
#define	ATH_HAL_GETRXBUF(_ah) \
	((*(_ah)->ah_getRxDP)((_ah)))
#define	ATH_HAL_TXSTART(_ah, _q) \
	((*(_ah)->ah_startTxDma)((_ah), (_q)))
#define	ATH_HAL_SETCHANNEL(_ah, _chan) \
	((*(_ah)->ah_setChannel)((_ah), (_chan)))
#define	ATH_HAL_CALIBRATE(_ah, _chan) \
	((*(_ah)->ah_perCalibration)((_ah), (_chan)))
#define	ATH_HAL_SETLEDSTATE(_ah, _state) \
	((*(_ah)->ah_setLedState)((_ah), (_state)))
#define	ATH_HAL_BEACONINIT(_ah, _nextb, _bperiod) \
	((*(_ah)->ah_beaconInit)((_ah), (_nextb), (_bperiod)))
#define	ATH_HAL_BEACONRESET(_ah) \
	((*(_ah)->ah_resetStationBeaconTimers)((_ah)))
#define	ATH_HAL_BEACONTIMERS(_ah, _beacon_state) \
	((*(_ah)->ah_setStationBeaconTimers)((_ah), (_beacon_state)))
#define	ATH_HAL_SETASSOCID(_ah, _bss, _associd) \
	((*(_ah)->ah_writeAssocid)((_ah), (_bss), (_associd)))
#define	ATH_HAL_SETOPMODE(_ah) \
	((*(_ah)->ah_setPCUConfig)((_ah)))
#define	ATH_HAL_STOPTXDMA(_ah, _qnum) \
	((*(_ah)->ah_stopTxDma)((_ah), (_qnum)))
#define	ATH_HAL_STOPPCURECV(_ah) \
	((*(_ah)->ah_stopPcuReceive)((_ah)))
#define	ATH_HAL_STARTPCURECV(_ah) \
	((*(_ah)->ah_startPcuReceive)((_ah)))
#define	ATH_HAL_STOPDMARECV(_ah) \
	((*(_ah)->ah_stopDmaReceive)((_ah)))
#define	ATH_HAL_DUMPSTATE(_ah) \
	((*(_ah)->ah_dumpState)((_ah)))
#define	ATH_HAL_DUMPEEPROM(_ah) \
	((*(_ah)->ah_dumpEeprom)((_ah)))
#define	ATH_HAL_DUMPRFGAIN(_ah) \
	((*(_ah)->ah_dumpRfGain)((_ah)))
#define	ATH_HAL_DUMPANI(_ah) \
	((*(_ah)->ah_dumpAni)((_ah)))
#define	ATH_HAL_SETUPTXQUEUE(_ah, _type, _irq) \
	((*(_ah)->ah_setupTxQueue)((_ah), (_type), (_irq)))
#define	ATH_HAL_RESETTXQUEUE(_ah, _q) \
	((*(_ah)->ah_resetTxQueue)((_ah), (_q)))
#define	ATH_HAL_RELEASETXQUEUE(_ah, _q) \
	((*(_ah)->ah_releaseTxQueue)((_ah), (_q)))
#define	ATH_HAL_HASVEOL \
	((*(_ah)->ah_hasVEOL)((_ah)))
#define	ATH_HAL_GETRFGAIN(_ah) \
	((*(_ah)->ah_getRfGain)((_ah)))
#define	ATH_HAL_RXMONITOR(_ah, _arg) \
	((*(_ah)->ah_rxMonitor)((_ah), (_arg)))
#define	ATH_HAL_SETUPBEACONDESC(_ah, _ds, _opmode, _flen, _hlen, \
		_rate, _antmode) \
	((*(_ah)->ah_setupBeaconDesc)((_ah), (_ds), (_opmode), \
		(_flen), (_hlen), (_rate), (_antmode)))
#define	ATH_HAL_SETUPRXDESC(_ah, _ds, _size, _intreq) \
	((*(_ah)->ah_setupRxDesc)((_ah), (_ds), (_size), (_intreq)))
#define	ATH_HAL_RXPROCDESC(_ah, _ds, _dspa, _dsnext) \
	((*(_ah)->ah_procRxDesc)((_ah), (_ds), (_dspa), (_dsnext)))
#define	ATH_HAL_SETUPTXDESC(_ah, _ds, _plen, _hlen, _atype, _txpow, \
		_txr0, _txtr0, _keyix, _ant, _flags, \
		_rtsrate, _rtsdura) \
	((*(_ah)->ah_setupTxDesc)((_ah), (_ds), (_plen), (_hlen), (_atype), \
		(_txpow), (_txr0), (_txtr0), (_keyix), (_ant), \
		(_flags), (_rtsrate), (_rtsdura)))
#define	ATH_HAL_SETUPXTXDESC(_ah, _ds, \
		_txr1, _txtr1, _txr2, _txtr2, _txr3, _txtr3) \
	((*(_ah)->ah_setupXTxDesc)((_ah), (_ds), \
		(_txr1), (_txtr1), (_txr2), (_txtr2), (_txr3), (_txtr3)))
#define	ATH_HAL_FILLTXDESC(_ah, _ds, _l, _first, _last, _ath_desc) \
	((*(_ah)->ah_fillTxDesc)((_ah), (_ds), (_l), (_first), (_last), \
	(_ath_desc)))
#define	ATH_HAL_TXPROCDESC(_ah, _ds) \
	((*(_ah)->ah_procTxDesc)((_ah), (_ds)))
#define	ATH_HAL_CIPHERSUPPORTED(_ah, _cipher) \
	(ATH_HAL_GETCAPABILITY(_ah, HAL_CAP_CIPHER, _cipher, NULL) == HAL_OK)
#define	ATH_HAL_TKIPSPLIT(_ah) \
	(ATH_HAL_GETCAPABILITY(_ah, HAL_CAP_TKIP_SPLIT, 0, NULL) == HAL_OK)

#ifdef __cplusplus
}
#endif

#endif /* _ATH_HAL_H */
