#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.3	04/11/05 SMI"
#
#cmd/ipf/Makefile
#

SUBDIRS=	etc examples \
		lib svc .WAIT \
		pfild tools

LINTCLEAN=
LINTSUBDIRS=

include ../Makefile.cmd

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

.KEEP_STATE:

.PARALLEL:      $(SUBDIRS)

all lint clean clobber: $(SUBDIRS)

install: $(SUBDIRS)

$(SUBDIRS): FRC
	cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

check:
