#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.3	04/11/05 SMI"
#

PR_MDUSOBJS = os_SunOS_x86.o

include ../Makefile.com
include ../../Makefile.lib.64

LDLIBS +=	-L$(NSS_LDPATH64) $(NSS_LIBS)
DYNFLAGS +=	-R$(NSS_LDPATH64)

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64) 
