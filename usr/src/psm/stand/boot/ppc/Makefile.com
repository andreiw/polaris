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
# Portions Copyright 2006 Noah Yan <noah.yan@gmail.com>
# 
#ident  "%Z%%M% %I%     %E% NY"
#

include $(TOPDIR)/psm/stand/boot/Makefile.boot

TARG_MACH	= ppc
PLATFORM	= chrp

BOOTSRCDIR	= ../..

TOP_CMN_DIR	= $(SRC)/common
CMN_DIR		= $(BOOTSRCDIR)/common
MACH_DIR	= ../../ppc/common
PLAT_DIR	= .
BOOT_DIR        = $(SRC)/psm/stand/boot

BOOT_SRC	= boot.c

CONF_SRC	= ufsconf.c nfsconf.c hsfsconf.c

TOP_CMN_C_SRC	= getoptstr.c

MISC_SRC	= ramdisk.c

CMN_C_SRC	= heap_kmem.c readfile.c

MACH_C_SRC	= boot_plat.c bootops.c bootprop.c boot_services.c bootflags.c
MACH_C_SRC	+= get.c

BOOT_OBJS	= $(BOOT_SRC:%.c=%.o)
BOOT_L_OBJS	= $(BOOT_OBJS:%.o=%.ln)

CONF_OBJS	= $(CONF_SRC:%.c=%.o)
CONF_L_OBJS	= $(CONF_OBJS:%.o=%.ln)

MISC_OBJS	= $(MISC_SRC:%.c=%.o)
MISC_L_OBJS	= $(MISC_OBJS:%.o=%.ln)

SRT0_OBJ	= $(SRT0_S:%.S=%.o)
SRT0_L_OBJ	= $(SRT0_OBJ:%.o=%.ln)

C_SRC		= $(TOP_CMN_C_SRC) $(CMN_C_SRC) $(MACH_C_SRC) $(ARCH_C_SRC)
C_SRC		+= $(PLAT_C_SRC)
S_SRC		= $(MACH_S_SRC) $(ARCH_S_SRC) $(PLAT_S_SRC)

OBJS		= $(C_SRC:%.c=%.o) $(S_SRC:%.s=%.o) $(S_SRC:%.S=%.o)
L_OBJS		= $(OBJS:%.o=%.ln)

BOOTCODE_OFFSET = 0x9000
# TEXT_OFFSETFLAGS = -Ttext $(BOOTCODE_OFFSET)
ENTRYFLAGS = -e_start
# COMM_LDFLAGS = -dn $(ENTRYFLAGS) $(TEXT_OFFSETFLAGS) -Bstatic
COMM_LDFLAGS = -nostdlib $(ENTRYFLAGS) $(TEXT_OFFSETFLAGS) -Bstatic

# This could be set in environment variables 
DEBUG_CPPFLAGS = -g -DDEBUG -DDEBUG_LISTS
#DEBUG_CPPFLAGS = -g -DDEBUG -DDEBUG_LISTS -DHALTBOOT

CPPDEFS		= $(ARCHOPTS) -D$(PLATFORM) -D_BOOT -D_KERNEL -D_MACHDEP $(DEBUG_CPPFLAGS)
#CPPDEFS	+= -D_ELF64_SUPPORT
CPPDEFS		+= -D_SYSCALL32
CPPDEFS		+= $(EXTRA_OPTIONS)
# CPPDEFS		+= -DLOADER_OFFSET=$(BOOTCODE_OFFSET)
CPPINCS		+= -I$(BOOT_DIR)/$(TARG_MACH)/common
CPPINCS		+= -I$(PSMSYSHDRDIR)
CPPINCS		+= -I$(SRC)/uts/$(TARG_MACH)
CPPINCS		+= -I$(SRC)/uts/$(PLATFORM)
CPPINCS		+= -I$(STANDDIR)
CPPINCS		+= -I$(STANDDIR)/lib
CPPINCS		+= -I$(STANDDIR)/lib/sa
CPPINCS		+= -I$(TOP_CMN_DIR)
CPPINCS		+= -I$(SRC)/uts/common
CPPINCS		+= -I$(SRC)/common/net
CPPINCS		+= -I$(SRC)/common/net/dhcp
CPPINCS		+= -I$(SRC)/common/net/crypt
# Comment below two to stop picking up userland headers which may cause conflicts with stand/sa
# CPPINCS		+= -I$(ROOT)/usr/platform/$(PLATFORM)/include
# CPPINCS		+= -I$(ROOT)/usr/include/$(ARCHVERS)
CPPFLAGS	= $(CPPDEFS) $(CPPINCS)
CPPFLAGS	+= $(CCYFLAG) $(STANDDIR)
ASFLAGS		+= -D_ASM
# ASFLAGS		+= $(CPPDEFS) -P -D_ASM $(CPPINCS)

PSMNAMELIBDIR	= $(PSMSTANDDIR)/lib/names/$(TARG_MACH)
PSMPROMLIBDIR	= $(PSMSTANDDIR)/lib/promif/$(TARG_MACH)

#
# The following libraries are built in LIBNAME_DIR
#
LIBNAME_DIR     += $(PSMNAMELIBDIR)/$(PLATFORM)
LIBNAME_LIBS    += libnames.a

#
# The following libraries are built in LIBPROM_DIR
#
LIBPROM_DIR     += $(PSMPROMLIBDIR)/$(PROMVERS)/common
LIBPROM_LIBS    += libprom.a

#
# The following libraries are built in LIBSYS_DIR
#
LIBSYS_DIR      += $(SYSLIBDIR)
LIBSYS_LIBS     += libufs.a libhsfs.a libnfs.a libxdr.a \
		libsock.a libinet.a libtcp.a libtcpstubs.a libscrypt.a \
		libsa.a libmd5.a libnvpair.a

#
# Used to convert ELF to an a.out and ensure alignment
#
STRIPALIGN = stripalign

#
# Program used to post-process the ELF executables
#
ELFCONV	= ./$(STRIPALIGN)			# Default value

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(CONF_OBJS) $(MISC_OBJS) $(SRT0_OBJ) $(BOOT_OBJS)
.PARALLEL:	$(L_OBJS) $(CONF_L_OBJS) $(MISC_L_OBJS) $(SRT0_L_OBJ) \
		$(BOOT_L_OBJS)
.PARALLEL:	$(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

all: $(ELFCONV) $(UFSBOOT) $(NFSBOOT)

$(STRIPALIGN): $(CMN_DIR)/$$(@).c
	echo "Later in a ppc box, change this to NATIVEGCC, instead of using SUNWspro cc!"
	$(NATIVECC) -o $@ $(CMN_DIR)/$@.c

# 4.2 ufs filesystem booter
#
# Libraries used to build ufsboot
#
LIBUFS_LIBS     = libufs.a libnames.a libsa.a libprom.a $(LIBPLAT_LIBS)
UFS_LIBS        = $(LIBUFS_LIBS:lib%.a=-l%)
UFS_DIRS        = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
UFS_DIRS        += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Note that the presumption is that someone has already done a `make
# install' from usr/src/stand/lib, such that all of the standalone
# libraries have been built and placed in $ROOT/stand/lib.
#
LIBDEPS=	$(LIBPROM_DIR)/libprom.a $(LIBPLAT_DEP) \
		$(LIBNAME_DIR)/libnames.a

L_LIBDEPS=	$(LIBPROM_DIR)/llib-lprom.ln $(LIBPLAT_DEP_L) \
		$(LIBNAME_DIR)/llib-lnames.ln

#
# Loader flags used to build ufsboot
#
UFS_MAPFILE	= $(MACH_DIR)/mapfile
UFS_LDFLAGS	= $(COMM_LDFLAGS) $(UFS_DIRS)
UFS_L_LDFLAGS	= $(UFS_DIRS)

#
# Object files used to build ufsboot
#
UFS_SRT0        = $(SRT0_OBJ)
UFS_OBJS        = $(OBJS) ufsconf.o boot.o
UFS_L_OBJS      = $(UFS_SRT0:%.o=%.ln) $(UFS_OBJS:%.o=%.ln)

#
# Build rules to build ufsboot
#

$(UFSBOOT).elf: $(UFS_MAPFILE) $(UFS_SRT0) $(UFS_OBJS) $(LIBDEPS)
	$(LD) $(UFS_LDFLAGS) -o $@ $(UFS_SRT0) $(UFS_OBJS) $(UFS_LIBS)
	$(MCS) -d $@
	$(POST_PROCESS)
	$(POST_PROCESS)
	$(MCS) -c $@

$(UFSBOOT): $(UFSBOOT).elf
	$(RM) $@; cp $@.elf $@
	$(STRIP) $@

$(UFSBOOT)_lint: $(L_LIBDEPS) $(UFS_L_OBJS)
	@echo ""
	@echo ufsboot lint: global crosschecks:
	$(LINT.c) $(UFS_L_LDFLAGS) $(UFS_L_OBJS) $(UFS_LIBS)

#
# Libraries used to build hsfsboot
#
LIBHSFS_LIBS    = libhsfs.a libnames.a libsa.a libprom.a $(LIBPLAT_LIBS)
HSFS_LIBS       = $(LIBHSFS_LIBS:lib%.a=-l%)
HSFS_DIRS       = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
HSFS_DIRS       += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build hsfsboot
#
HSFS_MAPFILE	= $(MACH_DIR)/mapfile
HSFS_LDFLAGS	= $(COMM_LDFLAGS) $(HSFS_DIRS)
HSFS_L_LDFLAGS	= $(HSFS_DIRS)

#
# Object files used to build hsfsboot
#
HSFS_SRT0       = $(SRT0_OBJ)
HSFS_OBJS       = $(OBJS) hsfsconf.o boot.o
HSFS_L_OBJS     = $(HSFS_SRT0:%.o=%.ln) $(HSFS_OBJS:%.o=%.ln)

$(HSFSBOOT).elf: $(HSFS_MAPFILE) $(HSFS_SRT0) $(HSFS_OBJS) $(LIBDEPS)
	$(LD) $(HSFS_LDFLAGS) -o $@ $(HSFS_SRT0) $(HSFS_OBJS) $(HSFS_LIBS)
	$(MCS) -d $@
	$(POST_PROCESS)
	$(POST_PROCESS)
	$(MCS) -c $@

$(HSFSBOOT): $(HSFSBOOT).elf
	$(RM) $(@); cp $@.elf $@
	$(STRIP) $@

$(HSFSBOOT)_lint: $(HSFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo hsfsboot lint: global crosschecks:
	$(LINT.c) $(HSFS_L_LDFLAGS) $(HSFS_L_OBJS) $(HSFS_LIBS)

# NFS booter

#
# Libraries used to build nfsboot
#
LIBNFS_LIBS     = libnfs.a libxdr.a libnames.a \
		libsock.a libinet.a libtcp.a libsa.a libprom.a \
		$(LIBPLAT_LIBS)
NFS_LIBS        = $(LIBNFS_LIBS:lib%.a=-l%)
NFS_DIRS        = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
NFS_DIRS        += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build inetboot
#
NFS_MAPFILE	= $(MACH_DIR)/mapfile
NFS_LDFLAGS	= $(COMM_LDFLAGS) $(NFS_DIRS)
NFS_L_LDFLAGS	= $(NFS_DIRS)

#
# Object files used to build inetboot
#
NFS_SRT0        = $(SRT0_OBJ)
NFS_OBJS        = $(OBJS) nfsconf.o boot.o
NFS_L_OBJS      = $(NFS_SRT0:%.o=%.ln) $(NFS_OBJS:%.o=%.ln)

$(NFSBOOT).elf: $(ELFCONV) $(NFS_MAPFILE) $(NFS_SRT0) $(NFS_OBJS) $(LIBDEPS)
	$(LD) $(NFS_LDFLAGS) -o $@ $(NFS_SRT0) $(NFS_OBJS) $(NFS_LIBS)
#	$(MCS) -d $@
#	$(POST_PROCESS)
#	$(POST_PROCESS)
#	$(MCS) -c $@

#
# This is a bit strange because some platforms boot elf and some don't.
# So this rule strips the file no matter which ELFCONV is used.
#
$(NFSBOOT): $(NFSBOOT).elf
	$(RM) $@.tmp; cp $@.elf $@.tmp; $(STRIP) $@.tmp
	$(RM) $@; $(ELFCONV) $@.tmp $@; $(RM) $@.tmp

$(NFSBOOT)_lint: $(NFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo inetboot lint: global crosschecks:
	$(LINT.c) $(NFS_L_LDFLAGS) $(NFS_L_OBJS) $(NFS_LIBS)

include $(BOOTSRCDIR)/Makefile.rules

install:

clean:
	$(RM) make.out lint.out
	$(RM) $(OBJS) $(CONF_OBJS) $(MISC_OBJS) $(BOOT_OBJS) $(SRT0_OBJ)
	$(RM) $(NFSBOOT).elf $(UFSBOOT).elf $(HSFSBOOT).elf
	$(RM) $(L_OBJS) $(CONF_L_OBJS) $(MISC_L_OBJS) $(BOOT_L_OBJS) \
	      $(SRT0_L_OBJ)

clobber: clean
	$(RM) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT) $(STRIPALIGN)

lint: $(UFSBOOT)_lint $(NFSBOOT)_lint

include $(BOOTSRCDIR)/Makefile.targ
