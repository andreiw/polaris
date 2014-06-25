#
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.1	03/12/15 SMI"
#
#cmd/ipf/lib/Makefile
#

include $(SRC)/lib/Makefile.lib

SUBDIRS = 	$(MACH)

$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all clean clobber install lint:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include $(SRC)/lib/Makefile.targ
include ../Makefile.ipf
