#! /usr/bin/sh
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
#
# ident	"@(#)inetd-upgrade.sh	1.5	05/06/08 SMI"
#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

# Start by cleaning out obsolete instances.  For each one that
# exists in the repository, remove it.
inetd_obsolete_instances="
	network/nfs/rquota:ticlts
	network/nfs/rquota:udp
	network/rexec:tcp
	network/rexec:tcp6
	network/rpc/gss:ticotsord
	network/rpc/mdcomm:tcp
	network/rpc/mdcomm:tcp6
	network/rpc/meta:tcp
	network/rpc/meta:tcp6
	network/rpc/metamed:tcp
	network/rpc/metamed:tcp6
	network/rpc/metamh:tcp
	network/rpc/metamh:tcp6
	network/rpc/rex:tcp
	network/rpc/rstat:ticlts
	network/rpc/rstat:udp
	network/rpc/rstat:udp6
	network/rpc/rusers:udp
	network/rpc/rusers:udp6
	network/rpc/rusers:ticlts
	network/rpc/rusers:tcp
	network/rpc/rusers:tcp6
	network/rpc/rusers:ticotsord
	network/rpc/rusers:ticots
	network/rpc/spray:ticlts
	network/rpc/spray:udp
	network/rpc/spray:udp6
	network/rpc/wall:ticlts
	network/rpc/wall:udp
	network/rpc/wall:udp6
	network/security/krb5_prop:tcp
	network/security/ktkt_warn:ticotsord
	network/shell:tcp
	network/shell:tcp6only
	platform/sun4u/dcs:tcp
	platform/sun4u/dcs:tcp6
"

for i in $inetd_obsolete_instances; do
	enable=`svcprop -p general/enabled $i`
	if [ $? = 0 ]; then
		# Instance found, so disable and delete
		svcadm disable $i
		svccfg delete $i
		if [ "$enable" = "true" ]; then
			# Instance was enabled, so enable the replacement.
			# We must do this here because the profile which
			# normally enables these is only applied on first
			# install of smf.
			s=`echo $i | cut -f1 -d:`
			svcadm enable $s:default
		fi
	fi
done


# The Following blocks of code cause the inetconv generated services to be
# re-generated, so that the latest inetconv modifications are applied to all
# services generated by it.

inetdconf_entries_file=/tmp/iconf_entries.$$

# Create sed script that prints out inetd.conf src line from inetconv generated
# manifest.
cat <<EOF > /tmp/inetd-upgrade.$$.sed
/propval name='source_line'/{
n
s/'//g
p
}
/from the inetd.conf(4) format line/{
n
p
}
EOF

# get list of inetconv generated manifests
inetconv_manifests=`/usr/bin/find /var/svc/manifest -type f -name \*.xml | \
    /usr/bin/xargs /usr/bin/grep -l "Generated by inetconv"`

# For each inetconv generated manifest determine the instances that should
# be disabled when the new manifests are imported, and generate a file with
# the inetd.conf entries from all the manifests for consumption by inetconv.

> $inetdconf_entries_file
inetconv_services=""
instances_to_disable=""

for manifest in $inetconv_manifests; do

	manifest_instances=`/usr/sbin/svccfg inventory $manifest | \
	    egrep "svc:/.*:.*"`
	manifest_service=`/usr/sbin/svccfg inventory $manifest | \
	    egrep -v "svc:/.*:.*"`

	instance_disabled=""
	default_enabled=""
	enabled=""

	for instance in $manifest_instances; do
		# if the instance doesn't exist in the repository skip it
		svcprop -q $instance
		if [ $? -ne 0 ]; then
			continue
		fi

		enabled=`svcprop -p general/enabled $instance`

		default_instance=`echo $instance | grep ":default"`
		if [ "$default_instance" != "" ]; then
			default_enabled=$enabled
		else
			# add all non-default instances to disable list
			instances_to_disable="$instances_to_disable \
			    $instance"
			if [ "$enabled" != "true" ]; then
				instance_disabled="true"
			fi
		fi
	done

	# if none of the manifest's instances existed, skip this manifest
	if [ "$enabled" = "" ]; then
		continue
	fi

	# If the default instance existed and was disabled, or if didn't
	# exist and one of the other instances was disabled, add the default
	# to the list of instances to disable.
	if [ "$default_enabled" = "false" -o "$default_enabled" = "" -a \
	    "$instance_disabled" = "true" ]; then
		instances_to_disable="$instances_to_disable \
		    $manifest_service:default"
	fi

	# add the manifest's inetd.conf src line to file for inetconv
	sed -n -f /tmp/inetd-upgrade.$$.sed $manifest >> \
	    $inetdconf_entries_file
done

rm /tmp/inetd-upgrade.$$.sed

# Run inetconv on generated file, overwriting previous manifests and values
# in repository.
/usr/sbin/inetconv -f -i $inetdconf_entries_file

# disable the necessary instances
for inst in $instances_to_disable; do
	svcadm disable $inst
done


# If there is a saved config file from upgrade, use it to enable services
saved_config=/etc/inet/inetd.conf.preupgrade

if [ -f ${saved_config} ]; then 
	/usr/sbin/inetconv -e -i ${saved_config}
fi

# Now convert the remaining entries in inetd.conf to service manifests
/usr/sbin/inetconv

# Now disable myself as the upgrade is done
svcadm disable network/inetd-upgrade

exit 0
