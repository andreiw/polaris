#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile.com	1.2	04/09/08 SMI"

LIBRARY= gssapi.a
VERS= .1

PLUG_OBJS=	gssapi.o	gssapiv2_init.o

PLUG_LIBS =	-lgss

# include common definitions
include ../../Makefile.com
