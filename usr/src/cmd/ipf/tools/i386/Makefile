#
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.1	03/12/15 SMI"
 
include ../Makefile.tools

LDLIBS	 += -L ../../lib/$(MACH) -lipf

all: $(PROG) $(IPFPROG)

install: all $(ROOTUSRSBINPROG32) $(ROOTUSRSBINLINKS) \
	 $(ROOTIPF32) $(ROOTIPFPROG32) $(ROOTIPFLINKS)
