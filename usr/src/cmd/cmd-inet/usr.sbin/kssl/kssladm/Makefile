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
# ident	"@(#)Makefile	1.1	05/11/12 SMI"
#
# cmd/cmd-inet/usr.sbin/kssl/kssladm/Makefile
#

PROG= kssladm

include $(SRC)/cmd/Makefile.cmd
include $(SRC)/lib/openssl/Makefile.openssl

OBJS =	\
    kssladm.o \
    kssladm_create.o \
    kssladm_delete.o \
    openssl_util.o

SRCS = $(OBJS:%.o=%.c)

ROOTUSRLIBPROG = $(PROG:%=$(ROOTLIB)/%)

.KEEP_STATE:

CFLAGS +=	$(CCVERBOSE)

CPPFLAGS += $(OPENSSL_CPPFLAGS)
DYNFLAGS += $(OPENSSL_DYNFLAGS)
LDFLAGS += $(OPENSSL_LDFLAGS)
LINTFLAGS += $(OPENSSL_LDFLAGS)

LDLIBS += -lnsl -lpkcs11 -lcrypto -lcryptoutil

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS) $(DYNFLAGS)
	$(POST_PROCESS)

install: all $(ROOTUSRLIBPROG)

clean:
	$(RM) $(OBJS)

check:
	$(CSTYLE) -pP $(SRCS)

lint:	lint_SRCS

include ../../../../Makefile.targ
