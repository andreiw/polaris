#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.8	06/03/06 SMI"
#
# uts/sun4v/sys/Makefile
#
# include global definitions
UTSBASE	= ../..

#
# include global definitions
#
include ../Makefile.sun4v

#
# Override defaults.
#
FILEMODE	 = 644
GROUP = bin

SUN4_HDRS=			\
	clock.h			\
	cmp.h			\
	cpc_ultra.h		\
	cpu_sgnblk_defs.h 	\
	ddi_subrdefs.h		\
	dvma.h			\
	eeprom.h		\
	fcode.h			\
	idprom.h		\
	intr.h			\
	intreg.h		\
	ivintr.h		\
	memlist_plat.h	 	\
	memnode.h		\
	nexusdebug.h		\
	prom_debug.h		\
	scb.h			\
	sun4asi.h		\
	tod.h			\
	trapstat.h		\
	vis.h			\
	vm_machparam.h		\
	x_call.h		\
	xc_impl.h		\
	zsmach.h

CLOSED_SUN4_HDRS=			\
	memtestio.h

HDRS=	\
	hypervisor_api.h	\
	hsvc.h			\
	machasi.h		\
	machclock.h		\
	machcpuvar.h		\
	mach_descrip.h		\
	machintreg.h		\
	machparam.h		\
	machsystm.h		\
	machthread.h		\
	mmu.h			\
	niagaraasi.h		\
	niagararegs.h		\
	pte.h			\
	prom_plat.h		\
	qcn.h			\
	traptrace.h

CLOSED_HDRS=	\
	memtestio_ni.h		\
	memtestio_v.h

ROOTHDRS=		$(HDRS:%=$(USR_PSM_ISYS_DIR)/%)
$(CLOSED_BUILD)ROOTHDRS += $(CLOSED_HDRS:%=$(USR_PSM_ISYS_DIR)/%)

SUN4_ROOTHDRS=		$(SUN4_HDRS:%=$(USR_PSM_ISYS_DIR)/%)
$(CLOSED_BUILD)SUN4_ROOTHDRS += $(CLOSED_SUN4_HDRS:%=$(USR_PSM_ISYS_DIR)/%)

ROOTDIR=		$(ROOT)/usr/share/src
ROOTDIRS=		$(ROOTDIR)/uts $(ROOTDIR)/uts/$(PLATFORM)

ROOTLINK=		$(ROOTDIR)/uts/$(PLATFORM)/sys
LINKDEST=		../../../../platform/$(PLATFORM)/include/sys

CHECKHDRS=		$(HDRS:%.h=%.check) \
			$(SUN4_HDRS:%.h=%.cmncheck)
$(CLOSED_BUILD)CHECKHDRS +=			\
	$(CLOSED_HDRS:%.h=%.check)		\
	$(CLOSED_SUN4_HDRS:%.h=%.cmncheck)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(SUN4_ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT \
		$(ROOTHDRS) .WAIT \
		$(SUN4_ROOTHDRS) .WAIT $(ROOTLINK)

check:	$(CHECKHDRS)

#
# install rules
#
$(USR_PSM_ISYS_DIR)/%:	../../sfmmu/sys/% $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(USR_PSM_ISYS_DIR)/%:	../../sun4/sys/% $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(USR_PSM_ISYS_DIR)/%:	$(CLOSED)/uts/sun4/sys/% $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(USR_PSM_ISYS_DIR)/%:	$(CLOSED)/uts/sun4v/sys/% $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir.root.bin)

# -r because this used to be a directory and is now a link.
$(ROOTLINK):	$(ROOTDIRS)
	-$(RM) -r $@; $(SYMLINK) $(LINKDEST) $@ $(CHOWNLINK) $(CHGRPLINK)

mon/%.check:	mon/%.h
	$(DOT_H_CHECK)

%.check:	../../sfmmu/sys/%.h
	$(DOT_H_CHECK)
%.check:	$(CLOSED)/uts/sun4v/sys/%.h
	$(DOT_H_CHECK)
%.cmncheck:	../../sun4/sys/%.h
	$(DOT_H_CHECK)
%.cmncheck:	$(CLOSED)/uts/sun4/sys/%.h
	$(DOT_H_CHECK)

FRC:

include ../../Makefile.targ
