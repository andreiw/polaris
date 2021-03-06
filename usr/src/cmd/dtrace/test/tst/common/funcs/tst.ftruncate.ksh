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
#ident	"@(#)tst.ftruncate.ksh	1.1	06/08/28 SMI"

script()
{
	$dtrace -q -o $tmpfile -s /dev/stdin <<EOF
	tick-10ms
	{
		printf("%d\n", i++);
	}

	tick-10ms
	/i == 10/
	{
		ftruncate();
	}

	tick-10ms
	/i == 20/
	{
		exit(0);
	}
EOF
}

dtrace=/usr/sbin/dtrace
tmpfile=/tmp/tst.ftruncate.$$

script
status=$?

cat $tmpfile
rm $tmpfile

exit $status
