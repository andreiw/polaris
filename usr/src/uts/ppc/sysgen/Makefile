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
# Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#

HDRS_MMU = sdr1.h batu.h batl.h sr.h pte0.h pte1.h
HDRS_MISC = pvr.h hid0.h hid1.h msr.h
HDRS = $(HDRS_MMU) $(HDRS_MISC)

%.h: %.fd
	rm -f $@ $@.e
	perl make-accessors.pl -intsize 32 $<  > $@  2> $@.e

test: $(HDRS)

def all: $(HDRS) ../sys/Makefile.hdrs
	chmod 644 $(HDRS)
	cp -p $(HDRS) ../sys

../sys/Makefile.hdrs:
	perl make-hdrs.pl > ../sys/Makefile.hdrs

clean clobber clean.lint:
	rm -f *.h *.h.e
	cd ../sys ; rm -f $(HDRS) Makefile.hdrs

install_h: $(HDRS) ../sys/Makefile.hdrs
	cp *.h ../sys
