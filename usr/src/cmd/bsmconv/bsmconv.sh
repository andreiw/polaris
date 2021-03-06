#! /bin/sh
#
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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)bsmconv.sh	1.31	06/08/07 SMI"
#

PROG=bsmconv
STARTUP=/etc/security/audit_startup
DEVALLOC=/etc/security/device_allocate
DEVMAPS=/etc/security/device_maps
TEXTDOMAIN="SUNW_OST_OSCMD"
export TEXTDOMAIN

# Perform required permission checks, depending on value of LOCAL_ROOT
# (whether we are converting the active OS or just alternative boot
# environments).
permission()
{
ZONE=`/sbin/zonename`
if [ ! "$ZONE" = "global" -a "$LOCAL_ROOT" = "true" ]
then
	form=`gettext "%s: ERROR: you must be in the global zone to run this script."`
	printf "${form}\n" $PROG
	exit 1
fi

WHO=`id | cut -f1 -d" "`
if [ ! "$WHO" = "uid=0(root)" ]
then
	form=`gettext "%s: ERROR: you must be super-user to run this script."`
	printf "${form}\n" $PROG
	exit 1
fi

RESP="x"
while [ "$RESP" != `gettext "y"` -a "$RESP" != `gettext "n"` ]
do
gettext "This script is used to enable the Basic Security Module (BSM).\n"
form=`gettext "Shall we continue with the conversion now? [y/n]"`
echo "$form \c"
read RESP
done

if [ "$RESP" = `gettext "n"` ]
then
	form=`gettext "%s: INFO: aborted, due to user request."`
	printf "${form}\n" $PROG
	exit 2
fi
}

# Do some sanity checks to see if the arguments to bsmconv
# are, in fact, root directories for clients.
sanity_check()
{
for ROOT in $@
do

	if [ -d $ROOT -a -w $ROOT -a -f $ROOT/etc/system -a -d $ROOT/usr ]
	then
		# There is a root directory to write to,
		# so we can potentially complete the conversion.
		:
	else
		form=`gettext "%s: ERROR: %s doesn't look like a client's root."`
		printf "${form}\n" $PROG $ROOT
		form=`gettext "%s: ABORTED: nothing done."`
		printf "${form}\n" $PROG
		exit 4
	fi
done
}

# bsmconvert
#	All the real work gets done in this function

bsmconvert()
{

# If there is no startup file to be read by /lib/svc/method/svc-auditd,
# then gripe about it.

form=`gettext "%s: INFO: checking startup file."`
printf "${form}\n" $PROG 

if [ ! -f ${ROOT}/${STARTUP} ]
then
	form=`gettext "%s: ERROR: no %s file."`
	printf "${form}\n" $PROG $STARTUP
	form=`gettext "%s: Continuing ..."`
	printf "${form}\n" $PROG
fi

# Disable volume manager from running on reboot.
cat >> ${ROOT}/var/svc/profile/upgrade <<SVC_UPGRADE
svcadm disable svc:/system/filesystem/volfs:default
SVC_UPGRADE

# store the current state of volfs service for restoring later
# in bsmunconv.sh
svcs -o state -H svc:/system/filesystem/volfs:default > \
			${ROOT}/etc/security/spool/vold.state

# Turn on auditing in the loadable module

form=`gettext "%s: INFO: turning on audit module."`
printf "${form}\n" $PROG
if [ ! -f ${ROOT}/etc/system ]
then
	echo "" > ${ROOT}/etc/system
fi

grep -v "c2audit:audit_load" ${ROOT}/etc/system > /tmp/etc.system.$$
echo "set c2audit:audit_load = 1" >> /tmp/etc.system.$$
mv /tmp/etc.system.$$ ${ROOT}/etc/system
grep "set c2audit:audit_load = 1" ${ROOT}/etc/system > /dev/null 2>&1
if [ $? -ne 0 ]
then
    form=`gettext "%s: ERROR: cannot 'set c2audit:audit_load = 1' in %s/etc/system"`
    printf "${form}\n" $PROG $ROOT
    form=`gettext "%s: Continuing ..."`
    printf "${form}\n" $PROG
fi

# Initialize device allocation

form=`gettext "%s: INFO: initializing device allocation."`
printf "${form}\n" $PROG
if [ -x /usr/bin/plabel ]
then
	# Trusted Extensions is installed. This is not currently done
	# for alternate boot environments.
	if [ -z "$ROOT" -o "$ROOT" = "/" ]
	then
		/usr/sbin/devfsadm -e
	fi
else
	if [ ! -f ${ROOT}/${DEVALLOC} ]
	then
		mkdevalloc > ${ROOT}/$DEVALLOC
	fi
	if [ ! -f ${ROOT}/${DEVMAPS} ]
	then
		mkdevmaps > ${ROOT}/$DEVMAPS
	fi
fi

# enable auditd at next boot.
cat >> ${ROOT}/var/svc/profile/upgrade <<SVC_UPGRADE
/usr/sbin/svcadm enable system/auditd
SVC_UPGRADE
}

# main loop

sanity_check $@
if [ $# -eq 0 ]
then
	# converting local root, perform all permission checks
	LOCAL_ROOT=true
	permission

	ROOT=
	bsmconvert

	echo
	gettext "The Basic Security Module is ready.\n"
	gettext "If there were any errors, please fix them now.\n"
	gettext "Configure BSM by editing files located in /etc/security.\n"
	gettext "Reboot this system now to come up with BSM enabled.\n"
else
	# determine if local root is being converted ("/" passed on
	# command line), if so, full permission check required
	LOCAL_ROOT=false
	for ROOT in $@
	do
		if [ "$ROOT" = "/" ]
		then
			LOCAL_ROOT=true
		fi
	done

	# perform required permission checks (depending on value of
	# LOCAL_ROOT)
	permission

	for ROOT in $@
	do
		form=`gettext "%s: INFO: converting boot environment %s ..."`
		printf "${form}\n" $PROG $ROOT
		bsmconvert $ROOT
		form=`gettext "%s: INFO: done with boot environment %s"`
		printf "${form}\n" $PROG $ROOT
	done
	echo
	gettext "The Basic Security Module is ready.\n"
	gettext "If there were any errors, please fix them now.\n"
	gettext "Configure BSM by editing files located in /etc/security\n"
	gettext "in the root directories of each host converted.\n"
	gettext "Reboot each system converted to come up with BSM active.\n"
fi

exit 0
