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
#ident	"@(#)Makefile.com	1.5	05/09/13 SMI"

.KEEP_STATE:

PROG = eftinfo
LOCALOBJS = eftinfo.o
OBJS = $(LOCALOBJS) $(COMMONOBJS)
SRCS = $(LOCALOBJS:.o=.c) $(COMMONSRCS)

CPPFLAGS += -I../common
CFLAGS += $(CTF_FLAGS)
LDLIBS += -lumem

all: $(PROG)

debug: $(PROG)

include $(SRC)/cmd/fm/eversholt/Makefile.esc.com

install: all $(ROOTPROG)

LINTSRCS += $(LOCALOBJS:%.o=../common/%.c)
LINTFLAGS = -mnux

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(CTFMRG)
	$(POST_PROCESS)

clean:
	$(RM) $(OBJS) y.output y.tab.c y.tab.h a.out core

clobber: clean
	$(RM) $(PROG)

esclex.o: escparse.o

%.o: ../common/%.c
	$(COMPILE.c) $<
	$(CTFCONVO)
