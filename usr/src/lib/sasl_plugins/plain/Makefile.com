#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile.com	1.2	04/09/08 SMI"

LIBRARY= plain.a
VERS= .1

PLUG_OBJS=	plain.o		plain_init.o

# include common definitions
include ../../Makefile.com
