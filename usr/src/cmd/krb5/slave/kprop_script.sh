#!/bin/sh
#
# Copyright (c) 2000 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)kprop_script.sh	1.1	01/03/19 SMI"

if [ $# -lt 1 ]
then
	exit 0
fi

/usr/sbin/kdb5_util dump /var/krb5/slave_datatrans

for kdc in $*
do
	/usr/lib/krb5/kprop -f /var/krb5/slave_datatrans $kdc
done
