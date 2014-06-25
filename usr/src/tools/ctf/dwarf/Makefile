#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.1	04/09/28 SMI"

include		../../Makefile.tools

SUBDIRS	= $(MACH)

all	:= TARGET= all
install	:= TARGET= install
clean	:= TARGET= clean
clobber	:= TARGET= clobber
lint	:= TARGET= lint

.KEEP_STATE:

install all clean clobber: $(SUBDIRS)

lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
