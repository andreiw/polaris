#
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.2	03/12/22 SMI"

include ../Makefile.tools
include ../../../Makefile.cmd.64

CFLAGS64 += -D_ELF64

LDLIBS64 += -L ../../lib/$(MACH64) -lipf

all: $(PROG) $(IPFPROG)

install: all $(ROOTIPF64) $(ROOTUSRSBINPROG64) \
	$(ROOTUSRSBINLINKS) $(ROOTIPFPROG64) $(ROOTIPFLINKS)
