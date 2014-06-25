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
# ident	"@(#)Makefile	1.7	05/06/08 SMI"
#
# psm/stand/bootblks/hsfs/ppc/chrp/Makefile
#
#
# Platform specific Makefile for the boot block.
#
# PLATFORM is the target for the binary installation.
#
# PLATFORM_INCS is a list of directories to look in for platform specific
#	header files to include
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.
#
# PROM_TYPE is the type of prom OBP, IEEE1275, etc...
#
# XXX No Platform dependent dir in sparc or 1386
BASEDIR		= ../../..
PLATFORM	= chrp
PLATLINKS	=
PLATFORM_INCS	=
PROM_TYPE	= IEEE1275

#include $(BASEDIR)/hsfs/Makefile.hsfs
#include $(BASEDIR)/obp-c/Makefile.rules

#CPPINCS		+= -I$(PLATFORM_INCS)

.KEEP_STATE:

all:	
#	$(PROG).elf
#	@-$(MV) $(PROG).elf $(PROG)

#include $(BASEDIR)/obp-c/Makefile.targ
#include $(BASEDIR)/Makefile.targ
