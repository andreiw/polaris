#! /usr/bin/perl -w
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
# Copyright (c) 2006 by Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident  "%Z%%M% %I%     %E% SMI"
#
#
# Generate usr/src/uts/ppc/sys/Makefile.hdrs
#
# Automatically populate it with all header files, including the other
# header files generated by scripts in usr/src/uts/ppc/sysgen.
#
# There might be some reason why some header files should be excluded,
# or the list of header files should be modified in some other way.
# For example, usr/src/uts/intel/sys/Makefile excludes hrtcntl.h and hrtsys.h.
# Any exclusions or any other logic for modifying the HDRS make variable
# should be codified here, rather than hand-editing
# usr/src/uts/ppc/sys/Makefile.
#
# A simple exclusion mechanism is already provided,
# -x <name>, but so far is not used.

%exclude = ();
$debug = 0;
$verbose = 0;
$commit = 1;
while (defined($ARGV[0]) && $ARGV[0] =~ /^-/) {
  $_ = $ARGV[0];
  if    ($_ eq '-d') {
    $debug = 1;
    }
  elsif (m/^-d(\d+)$/) {
    $debug = $1;
    }
  elsif ($_ eq '-v') {
    $verbose = 1;
    }
  elsif ($_ eq '-n') {
    $commit = 0;
    }
  elsif ($_ eq '-x') {
    shift;
    $exclude{$ARGV[0]} = 1;
    }
  elsif ($_ eq '-') {
    last;
    }
  elsif ($_ eq '--') {
    shift;
    last;
    }
  else {
    die "Unknown option, '$_'\n ";
    }
  shift;
  }
shift  if (scalar(@ARGV)>0 && !defined($ARGV[0]));

# Create a list of all header files, except those that are excluded.
#
@hdr_list = ();
$dir = '../sys';
opendir(LS, $dir) || die "opendir('${dir}') failed; $!";
while ( (defined($ent = readdir(LS))) ) {
    # If it is a header file and it has not been explicitly excuded,
    # then add it to the list of header files.
    if ($ent =~ m/\.h$/ && !defined($exclude{$ent})) {
        push(@hdr_list, $ent);
        }
    }
closedir(LS);

# Compose Makefile.hdrs.
# I don't think we need to bother with the CDDL license
# for generated files.
#
# Have the coutesy to warn that this file is generated.
# The rest is just a variable assignment, build from
# the sorted list of header files.
#
print
  "# This file has been automatically generated.\n",
  "# DO NOT EDIT.\n",
  "# See usr/src/uts/ppc/sysgen/Makefile and helper scripts.\n",
  "# for more information on how this file was made, and how\n",
  "# int can best be maintained.\n",
  "#\n",
  "\n",
  "HDRS = \\\n";

for (sort @hdr_list) {
  print "\t$_ \\\n";
  }
print "\t###\n";

exit 0;
