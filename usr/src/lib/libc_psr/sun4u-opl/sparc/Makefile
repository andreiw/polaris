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
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.3	06/08/03 SMI"
#

#
# Platform specific Makefile for libc_psr.
#

PLATFORM	= sun4u-opl
COMPAT_PLAT	= sun4u-us3
CLASS		= 32

OBJECTS		= memcpy.o memset.o memcmp.o

include ../Makefile.com

MAPFILES =	../../sun4u/mapfile-vers $(MAPFILE-FLTR)

ASFLAGS	+=	-xarch=v8plusb

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS)

.KEEP_STATE:

MODULE		= libc_psr.so.1
LIBC_PSR_DIR    = $(ROOT_PSM_LIB_DIR)
LIBC_PSR_LIB    = $(LIBC_PSR_DIR)/$(MODULE)

$(LIBC_PSR_DIRS):
	-$(INS.dir.root.bin)

all:		$(LIBS)

install:	all $(LIBC_PSR_DIR) $(ROOT_PSM_LIBS)

include ../../Makefile.targ
