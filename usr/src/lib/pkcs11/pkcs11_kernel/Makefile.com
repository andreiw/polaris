#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile.com	1.12	06/08/01 SMI"
#

LIBRARY= pkcs11_kernel.a
VERS= .1

OBJECTS= \
	kernelGeneral.o 	\
	kernelSlottable.o 	\
	kernelSlotToken.o 	\
	kernelObject.o 		\
	kernelDigest.o	 	\
	kernelSign.o 		\
	kernelVerify.o 		\
	kernelDualCrypt.o 	\
	kernelKeys.o 		\
	kernelRand.o		\
	kernelSession.o		\
	kernelSessionUtil.o	\
	kernelUtil.o		\
	kernelEncrypt.o		\
	kernelDecrypt.o		\
	kernelObjectUtil.o	\
	kernelAttributeUtil.o

AESDIR=		$(SRC)/common/crypto/aes
BLOWFISHDIR=	$(SRC)/common/crypto/blowfish
ARCFOURDIR=	$(SRC)/common/crypto/arcfour
DESDIR=		$(SRC)/common/crypto/des

lint \
pics/kernelAttributeUtil.o := \
	CPPFLAGS += -I$(AESDIR) -I$(BLOWFISHDIR) -I$(ARCFOURDIR) -I$(DESDIR)

include $(SRC)/lib/Makefile.lib

#	set signing mode
POST_PROCESS_SO	+=	; $(ELFSIGN_CRYPTO)

SRCDIR=		../common

LIBS	=	$(DYNLIB)
LDLIBS  +=      -lc -lcryptoutil

CFLAGS  +=      $(CCVERBOSE)  

ROOTLIBDIR=     $(ROOT)/usr/lib/security
ROOTLIBDIR64=   $(ROOT)/usr/lib/security/$(MACH64)

.KEEP_STATE:

all:    $(LIBS)

lint: lintcheck

include $(SRC)/lib/Makefile.targ
