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
#ident	"@(#)Makefile.com	1.3	05/06/08 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/hsfs/common/Makefile.com
#

THISDIR = $(BASEDIR)/hsfs/common

#
# Files that define the fs-reading capabilities of the C-based boot block
#
# uncomment following line if want to build big bootblk
# FS_C_SRCS	= hsfs.c
# uncomment following line if want to build small bootblk
FS_C_SRCS	= hsfs_small.c

#
# Allow hsfs-specific files to find hsfs-specific include files
#
$(FS_C_SRCS:%.c=%.o)	:=	CPPINCS += -I$(THISDIR)

#
# Pattern-matching rules for source in this directory
#
%.o: $(THISDIR)/%.c
	$(COMPILE.c) -o $@ $<

%.ln: $(THISDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	
%.fcode: $(THISDIR)/%.fth
	$(TOKENIZE) $<