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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Portions Copyright 2006 Noah Yan <noah.yan@gmail.com>
#
# ident  "%Z%%M% %I%     %E% SMI"
#
# psm/stand/lib/promif/ppc/ieee1275/chrp/Makefile

TOPDIR =	../../../../../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PLATDIR =	$(TOPDIR)/psm/promif/ieee1275/chrp
PLATODWDIR = 	$(TOPDIR)/psm/promif/ieee1275/odw
PLATMACPPCDIR =	$(TOPDIR)/psm/promif/ieee1275/macppc
SYSDIR =	$(TOPDIR)/uts

LIBPLAT =	libplat.a
LINTLIBPLAT =	llib-lplat.ln

PLAT_PODWFILES = \
	      # Those in PLATFORMODWDIR or PLATMACPPCDIR
	      # Currently empty 

# Here since we are building a library, some source files may not needed
# but now, we include all the sources to make sure everything is built ok.
PLAT_PFILES =		\
	prom_alloc.c		\
	prom_dma.c		\
	prom_init.c		\
	prom_mem.c		\
	prom_mmu.c		\
	prom_ppc.c		\
	prom_macaddr.c	

OBJSDIR =	objs

PLAT_POBJ =	$(PLAT_PFILES:%.c=$(OBJSDIR)/%.o)
# PLAT_PSUN4OBJ =	$(PLAT_PSUN4FILES:%.c=$(OBJSDIR)/%.o)
OBJS =		$(PLAT_POBJ) # $(PLAT_PSUN4OBJ)
L_OBJS =	$(OBJS:%.o=%.ln)
L_SRCS = 	$(PLAT_PFILES:%=$(PLATDIR)/%)

ARCHOPTS=	-Dchrp
ASFLAGS =	-P -D__STDC__ -D_BOOT -D_ASM
CPPDEFS	=	$(ARCHOPTS) -D_BOOT -D_KERNEL -D_MACHDEP $(DEBUG_CPPFLAGS)
CPPINCS =	-I. -I$(SYSDIR)/ppc -I$(SYSDIR)/chrp -I$(SYSDIR)/common
CPPFLAGS=	$(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
CFLAGS +=	$(CCVERBOSE) $(EXTRA_OPTIONS)

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS)

all install: $(LIBPLAT)

lint: $(LINTLIBPLAT)

clean:
	$(RM) $(OBJS) $(L_OBJS)

clobber: clean
	$(RM) $(LIBPLAT) $(LINTLIBPLAT) a.out core

$(LIBPLAT): $(OBJSDIR) .WAIT $(OBJS)
	$(BUILD.AR) $(OBJS)

$(LINTLIBPLAT): $(OBJSDIR) .WAIT $(L_OBJS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o plat $(L_SRCS)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# build rules using standard library object subdirectory
#
$(OBJSDIR)/%.o: $(PLATDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.o: $(PLATDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

$(OBJSDIR)/%.ln: $(PLATDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	@$(MV) $(@F) $@

$(OBJSDIR)/%.ln: $(PLATDIR)/%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))
	@$(MV) $(@F) $@
