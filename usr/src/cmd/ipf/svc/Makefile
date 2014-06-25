#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.2	05/10/12 SMI"
#

MANIFEST=	ipfilter.xml pfil.xml 
SVCMETHOD=	ipfilter pfil

include $(SRC)/cmd/Makefile.cmd

ROOTMANIFESTDIR=	$(ROOTSVCNETWORK)
$(ROOTMANIFEST)		:= FILEMODE= 444

include $(SRC)/cmd/Makefile.targ

all: $(OBJS)

$(ROOTMANIFESTDIR)/%: %
		$(INS.file)

clean:
		$(RM) $(OBJS)

lint:		$(SRCS) lint_SRCS

install:	$(ROOTMANIFEST) $(ROOTSVCMETHOD)

check:		$(CHKMANIFEST)
