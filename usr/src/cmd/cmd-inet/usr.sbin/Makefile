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
# ident	"@(#)Makefile	1.126	06/03/24 SMI"
#

SYNCPROG=	syncinit syncloop syncstat
DHCPPROG=	dhcpconfig dhtadm pntadm

# EXPORT DELETE START
XMODPROG=	wanbootutil
# EXPORT DELETE END

PROG=		6to4relay arp gettable if_mpadm ikeadm ikecert \
		in.comsat in.fingerd in.rarpd in.rdisc in.rexecd in.rlogind \
		in.rshd in.rwhod in.telnetd in.tftpd in.tnamed ipaddrsel \
		ipsecalgs ipsecconf ipseckey ndd $(SYNCPROG) $(DHCPPROG) \
		$(XMODPROG)

MANIFEST=	rarp.xml telnet.xml comsat.xml finger.xml \
		login.xml shell.xml rexec.xml tname.xml

ROOTFS_PROG=	hostconfig route routeadm soconfig
SBINLINKS=	hostconfig route routeadm

RPCSVCPROG=	hostconfig
AUDITPROG=	in.rexecd in.rlogind in.rshd in.telnetd
PAMPROG=	in.rexecd in.rlogind in.rshd in.telnetd
SOCKETPROG=	6to4relay arp gettable hostconfig if_mpadm in.comsat \
		in.fingerd in.rarpd in.rdisc in.rexecd in.rlogind in.rshd \
		in.rwhod in.telnetd in.tftpd in.tnamed ipaddrsel \
		ipsecalgs ipsecconf ipseckey route routeadm
NSLPROG=	6to4relay arp gettable hostconfig ikeadm in.comsat in.rarpd \
		in.rdisc in.rexecd in.rlogind in.rshd in.rwhod in.telnetd \
		in.tftpd in.tnamed ipaddrsel ipsecalgs ipsecconf ipseckey route
CMDPROG=	in.telnetd
IPSECUTILPROG=	ikeadm ipsecalgs ipsecconf ipseckey
K5PROGS=	in.telnetd in.rlogind in.rshd
TSNETPROG=	route
DEFAULTFILES=	telnetd.dfl

PROGSRCS=	$(PROG:%=%.c)
TFTPDOBJS=	in.tftpd.o tftpsubs.o
OTHERSRC=	../usr.bin/tftp/tftpsubs.c
K5RLOGINOBJS=	in.rlogind.o
K5RSHDOBJS=	in.rshd.o
SRCS=		$(PROGSRCS) $(OTHERSRC)

SUBDIRS=	bootconfchk htable ifconfig in.ftpd in.routed \
		in.talkd inetadm inetconv ipqosconf kssl/kssladm \
		kssl/ksslcfg  mipagentconfig mipagentstat ping snoop \
		sppptun traceroute wificonfig

MSGSUBDIRS=	bootconfchk htable ifconfig in.ftpd in.routed in.talkd \
		inetadm inetconv ipqosconf kssl/ksslcfg mipagentconfig \
		mipagentstat sppptun snoop wificonfig

# As programs get lint-clean, add them here and to the 'lint' target.
# Eventually this hack should go away, and all in PROG should be
# lint-clean.
LINTCLEAN=	6to4relay arp ikeadm in.rlogind in.rshd in.telnetd in.tftpd \
		ipaddrsel ipsecalgs ipseckey ipsecconf route routeadm \
		in.rarpd if_mpadm $(SYNCPROG)
# Likewise, as subdirs get lint-clean, add them here.  Once
# they're all clean, replace the dependency of the lint target
# with SUBDIRS.  Also (sigh) deal with the commented-out build lines
# for the lint rule.
LINTSUBDIRS=	bootconfchk in.routed in.talkd inetadm inetconv ipqosconf \
		mipagentstat ping sppptun traceroute wificonfig
# And as programs are verified not to attempt to write into constants,
# -xstrconst should be used to ensure they stay that way.
CONSTCLEAN=	ikeadm

include ../../Makefile.cmd
ROOTMANIFESTDIR=	$(ROOTSVCNETWORK)
$(ROOTMANIFEST)		:= FILEMODE= 444
include ../Makefile.cmd-inet

ROOTSBINPROG = $(ROOTFS_PROG:%=$(ROOTSBIN)/%)
ROOTUSRSBINLINKS = $(SBINLINKS:%=$(ROOTUSRSBIN)/%)

COMMONOBJS=	kcmd.o
COMMONSRCS=	$(CMDINETCOMMONDIR)/$(COMMONOBJS:.o=.c)
SRCS+=		$(COMMONSRCS)

#
# Message catalog
#
POFILES=	6to4relay.po if_mpadm.po ikeadm.po in.comsat.po ipaddrsel.po \
		ipsecalgs.po ipsecconf.po ipseckey.po route.po routeadm.po
POFILE=		usr.sbin.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

CLOBBERFILES +=	$(ROOTFS_PROG) $(PROG)
CLEANFILES += $(COMMONOBJS) $(K5RLOGINOBJS) $(K5RSHDOBJS) $(TFTPDOBJS)

CPPFLAGS +=	-DSYSV -DBSD_COMP -I$(CMDINETCOMMONDIR) -I.

include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5
K5LIBS=

# Eventually just plain CFLAGS should be += -v, but not until all in
# PROGS are lint clean.
$(LINTCLEAN)		:=	CFLAGS += $(CCVERBOSE)
$(CONSTCLEAN)		:=	CFLAGS += $(XSTRCONST)

$(SYNCPROG)		:=	LDLIBS += -ldlpi
$(SOCKETPROG)		:=	LDLIBS += -lsocket
$(NSLPROG)		:=	LDLIBS += -lnsl
$(AUDITPROG)		:=	LDLIBS += -lbsm
$(PAMPROG)		:=	LDLIBS += -lpam
$(RPCSVCPROG)		:=	LDLIBS += -lrpcsvc
$(CMDPROG)		:=	LDLIBS += -lcmd
$(K5PROGS)		:=	LDFLAGS += $(ZLAZYLOAD) $(KRUNPATH) \
				-L$(ROOT)$(KLIBDIR_DO) -L$(ROOT)$(KLIBDIR_GL)
$(K5PROGS)		:=	K5LIBS=  -lmech_krb5
$(K5PROGS)		:=	CPPFLAGS += -I$(SRC)/head \
				-I$(SRC)/uts/common/ \
				-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
				-I$(SRC)/lib/gss_mechs/mech_krb5/include \
				-I$(SRC)/lib/pam_modules/krb5
LDLIBS +=	$(K5LIBS)
$(IPSECUTILPROG)	:=	LDLIBS += -lipsecutil
$(TSNETPROG)		:=	LDLIBS += -ltsnet

in.rarpd		:=	LDLIBS += -linetutil
route			:=	CPPFLAGS += -DNDEBUG

.KEEP_STATE:

.PARALLEL:

all: $(PROG) $(ROOTFS_PROG) $(SUBDIRS)

#
# message catalog
#
_msg: $(MSGSUBDIRS) $(POFILE)

syncutil: $(SYNCPROG)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

$(COMMONOBJS): $(COMMONSRCS)
	$(COMPILE.c) $(COMMONSRCS)

in.rlogind: $(K5RLOGINOBJS) $(COMMONOBJS)
	$(LINK.c) $(K5RLOGINOBJS) $(COMMONOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

in.rshd: $(K5RSHDOBJS) $(COMMONOBJS)
	$(LINK.c) $(K5RSHDOBJS) $(COMMONOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

in.tftpd: $(TFTPDOBJS)
	$(LINK.c) $(TFTPDOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

tftpsubs.o: $(OTHERSRC)
	$(COMPILE.c) $(OTHERSRC) -o $@
	$(POST_PROCESS_O)

$(ROOTUSRSBINLINKS):
	-$(RM) $@; $(SYMLINK) ../../sbin/$(@F) $@

install: $(PROG) $(ROOTFS_PROG) $(SUBDIRS) .WAIT $(ROOTUSRSBINPROG) \
	$(ROOTSBINPROG) $(ROOTUSRSBINLINKS) $(ROOTETCDEFAULTFILES) \
	$(ROOTMANIFEST)

#
# The reason this rule checks for the existence of the
# Makefile is that some of the directories do not exist
# in our exportable source builds.
#
$(SUBDIRS): FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

FRC:

check:	$(CHKMANIFEST)

clean:	$(SUBDIRS)
	-$(RM) $(CLEANFILES)

clobber: $(SUBDIRS)
	-$(RM) $(CLEANFILES) $(CLOBBERFILES)

lint: $(LINTSUBDIRS)
	$(LINT.c) 6to4relay.c $(LDLIBS) -lsocket -lnsl
	$(LINT.c) arp.c $(LDLIBS) -lsocket -lnsl
	@# $(LINT.c) in.rexecd.c $(LDLIBS) -lbsm -lpam
	$(LINT.c) -erroff=E_NAME_USED_NOT_DEF2 -erroff=E_NAME_DEF_NOT_USED2 \
		-I$(SRC)/head -I$(SRC)/uts/common/ \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib/pam_modules/krb5 \
		in.rlogind.c $(COMMONSRCS) $(LDLIBS) -lbsm -lpam -lsocket -lnsl
	$(LINT.c) -erroff=E_NAME_USED_NOT_DEF2 -erroff=E_NAME_DEF_NOT_USED2 \
		-I$(SRC)/head -I$(SRC)/uts/common/ \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib/pam_modules/krb5 \
		in.rshd.c $(COMMONSRCS) $(LDLIBS) -lbsm -lpam -lsocket -lnsl
	$(LINT.c) -erroff=E_NAME_USED_NOT_DEF2 \
		-erroff=E_GLOBAL_COULD_BE_STATIC2 \
		-I$(SRC)/head -I$(SRC)/uts/common/ \
		-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
		-I$(SRC)/lib/gss_mechs/mech_krb5/include \
		-I$(SRC)/lib/pam_modules/krb5 \
		in.telnetd.c $(LDLIBS) -lbsm -lpam -lsocket -lnsl
	$(LINT.c) if_mpadm.c $(LDLIBS) -lsocket -lnsl -lipmp
	$(LINT.c) ipaddrsel.c $(LDLIBS) -lsocket -lnsl
	$(LINT.c) ipsecalgs.c $(LDLIBS) -lsocket -lnsl -lipsecutil
	$(LINT.c) ipsecconf.c $(LDLIBS) -lsocket -lnsl -lipsecutil
	$(LINT.c) ipseckey.c $(LDLIBS) -lsocket -lnsl -lipsecutil
	$(LINT.c) ikeadm.c $(LDLIBS) -lnsl -lipsecutil
	$(LINT.c) route.c $(LDLIBS) -lsocket -lnsl -ltsnet
	$(LINT.c) routeadm.c $(LDLIBS) -lsocket
	$(LINT.c) syncinit.c $(LDLIBS) -ldlpi
	$(LINT.c) syncloop.c $(LDLIBS) -ldlpi
	$(LINT.c) syncstat.c $(LDLIBS) -ldlpi
	$(LINT.c) -erroff=E_NAME_USED_NOT_DEF2 in.rarpd.c $(LDLIBS) \
	    -lsocket -lnsl
	$(LINT.c) in.tftpd.c ../usr.bin/tftp/tftpsubs.c $(LDLIBS) \
	    -lsocket -lnsl

# EXPORT DELETE START
EXPORT_SRC:
	$(RM) Makefile+
	sed -e "/^# EXPORT DELETE START/,/^# EXPORT DELETE END/d" \
		< Makefile > Makefile+
	$(RM) Makefile
	$(MV) Makefile+ Makefile
	$(CHMOD) 444 Makefile
# EXPORT DELETE END
