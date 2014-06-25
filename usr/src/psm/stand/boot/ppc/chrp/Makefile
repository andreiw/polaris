#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Portions Copyright (c) 2006 Noah Yan <noah.yan@gmail.com>
#
#ident  "%Z%%M% %I%     %E% NY"
#

TOPDIR =	../../../../..
PLATFORM =	chrp
KARCH =		$(PLATFORM)
KARCHVERS =

# include $(TOPDIR)/uts/Makefile.uts	# also nested include Makefile.master

MODULE	= fs

PLATLINKS	= MACPPC
PLATLINKS	+= PEGASOS 

LINKED_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_LIB_FS_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib/fs)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean

TARG_MACH =	ppc
TARG_MACH_DIR =	ppc
PROMVERS =	ieee1275

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =	chrp_memlist.c chrp_standalloc.c chrp_dep.c
# ARCH_S_SRC =	chrp_subr.S
SRT0_S =	chrp_srt0.S
INLINES =

LDFLAGS +=	-L$(TOPDIR)/psm/stand/lib/promif/$(TARG_MACH)/$(PROMVERS)/common

OBJSDIR =	objs

#
# The following libraries are build in LIBPLAT_DIR
#
LIBPLAT_DIR =	$(TOPDIR)/psm/stand/lib/promif/$(TARG_MACH)/$(PROMVERS)/$(PLATFORM)
LIBPLAT_LIBS =  libplat.a 
LIBPLAT_L_LIBS=	$(LIBPLAT_LIBS:lib%.a=llib-l%.ln)
LIBPLAT_DEP = 	$(LIBPLAT_DIR)/$(LIBPLAT_LIBS)
LIBPLAT_DEP_L =	$(LIBPLAT_DIR)/$(LIBPLAT_L_LIBS)

#
# Platform specific libraries
#
PSMLIBS +=      $(LIBPLAT_LIBS:lib%.a=-l%)
PSMLIB_DIRS +=  $(LIBPLAT_DIR)


#	MPSAS support
MPSAS_BUILD:sh=		echo \\043
$(MPSAS_BUILD)ARCHOPTS += -DMPSAS

include ../Makefile.com	# also nest include Makefile.master through Makefile.boot, so is the include of Makefile.uts required??

ELFCONV = /usr/bin/cp

# Don't need mapfile.inet for ppc inetboot
NFS_MAPFILE = $(MACH_DIR)/mapfile

#
# Set the choice of compiler. 

# include $(TOPDIR)/psm/Makefile.psm.64

# CFLAGS64	+= -xchip=ultra $(CCABS32)

#
# XXX this totally sucks since it effectively turns off -errchk=longptr64,
# which we really should be using.
#
#
# Cross-reference customization: include all boot-related source files.
#
UTSDIR=		../../../../../uts
STANDLIBDIR=	../../../../../stand/lib
STANDSYSDIR=	../../../../../stand/sys
PROMDIRS=	../../../../promif
NAMESDIRS=	../../../lib/names/ppc

XRDIRS +=	../common $(STANDLIBDIR) \
		$(STANDSYSDIR) $(PROMDIRS) $(NAMESDIRS)
XRDEL =		chrp_srt0* chrp_memlist*
XRPRUNE =	i86pc i386 sparc

cscope.out tags: FRC
	$(XREF) -x $@

FRC:
