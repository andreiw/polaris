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
#pragma ident	"@(#)Makefile	1.3	05/06/08 SMI"
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Makefile for logical volume management 
#
#
# cmd/lvm/rpc.metamhd/Makefile

PROG= rpc.metamhd 
MANIFEST=	metamh.xml

RPCMOD = metamhd

include ../../Makefile.cmd
include ../Makefile.lvm

ROOTMANIFESTDIR=	$(ROOTSVCNETWORKRPC)
$(ROOTMANIFEST)		:= FILEMODE= 444

SUBDIRS=        $(MACH)

all     :=      TARGET = all
install :=      TARGET = install
clean   :=      TARGET = clean
clobber :=      TARGET = clobber
lint    :=      TARGET = lint

.KEEP_STATE:

all:    $(SUBDIRS)

clean: $(SUBDIRS)
	$(RM) metamhd_svc.c metamhd_xdr.c mhdx_xdr.c

clobber lint:     $(SUBDIRS)

install:        $(SUBDIRS) $(ROOTMANIFEST)
	-$(RM)  $(ROOTPROG)

check:	$(CHKMANIFEST)

$(SUBDIRS):     FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../../Makefile.targ
