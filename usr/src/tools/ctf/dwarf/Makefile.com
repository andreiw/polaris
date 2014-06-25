#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile.com	1.1	04/09/28 SMI"

include ../../Makefile.ctf

.KEEP_STATE:
.PARALLEL:

all:	libdwarf.so

install: all $(ROOTONBLDLIBMACH)/libdwarf.so.1

clean clobber:
	$(RM) libdwarf.so

FILEMODE	= 0755

%.so: %.so.1
	-$(RM) $@ ; \
	$(SYMLINK) ./$< $@

$(ROOTONBLDLIBMACH)/%: %
	$(INS.file)
