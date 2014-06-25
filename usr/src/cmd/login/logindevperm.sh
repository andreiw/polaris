#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)logindevperm.sh	1.7	05/06/10 SMI"
#
#
# This is the script that generates the logindevperm file. It is
# architecture-aware, and dumps different stuff for x86 and sparc.
# There is a lot of common entries, which are dumped first.
#
# the SID of this script, and the SID of the dumped script are
# always the same.
#

cat <<EOM
#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#pragma ident	"@(#)logindevperm	1.7	05/06/10 SMI"
#
# /etc/logindevperm - login-based device permissions
#
# If the user is logging in on a device specified in the "console" field
# of any entry in this file, the owner/group of the devices listed in the
# "devices" field will be set to that of the user.  Similarly, the mode
# will be set to the mode specified in the "mode" field.
#
# "devices" is a colon-separated list of device names.  A device name
# ending in "/*", such as "/dev/fbs/*", specifies all entries (except "."
# and "..") in a directory.  A '#' begins a comment and may appear
# anywhere in an entry.
# In addition, regular expressions may be used. Refer to logindevperm(4)
# man page.
# Note that any changes in this file should be made when logged in as
# root as devfs provides persistence on minor node attributes.
#
# console	mode	devices
#
/dev/console	0600	/dev/mouse:/dev/kbd
/dev/console	0600	/dev/sound/*		# audio devices
/dev/console	0600	/dev/fbs/*		# frame buffers
EOM

case "$MACH" in
    "i386" )
	# 
	# These are the x86 specific entries
	# It depends on the build machine being an x86
	#
	cat <<-EOM
	EOM
	;;
    "sparc" )
	# 
	# These are the sparc specific entries
	# It depends on the build machine being a sparc
	#
	cat <<-EOM
	EOM
	;;
    "ppc" )
	# 
	# These are the ppc specific entries
	# It depends on the build machine being a ppc
	#
	cat <<-EOM
	EOM
	;;
    * )
	echo "Unknown Architecture"
		exit 1
	;;
esac
