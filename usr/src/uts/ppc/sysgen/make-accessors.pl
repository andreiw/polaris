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

$cmd_pfx = ':';
$int_size = 0;
$modify = 1;
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
  elsif ($_ eq '-ro') {
    $modify = 0;
    }
  elsif ($_ eq '-intsize') {
    shift;
    $int_size = $ARGV[0];
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

if (@ARGV == 0) {
  @ARGV = ( '-' );
  }

for (@ARGV) {
  print STDERR  '  ', $_, "\n"  if ($verbose);
  %T = ();
  eval { &do_file($_); };
  if ($@) {
    print STDERR  $@;
    }
  unlink(keys %T)  unless ($debug);
  }

exit ($err_count != 0);

sub do_file {
  local($src_fname) = @_;
  local($tmp_fname, $tmp_open);

  open(SRC, $src_fname) || die "open('${src_fname}') failed; $!";
  $tmp_fname = '/tmp/' . $$ . '.1.mka';
  $tmp_open = '> ' . $tmp_fname;
  open(TMP, $tmp_open) || die "open('${tmp_open}') failed; $!";
  $T{$tmp_fname} = 1;

  &init_scan;
  &init_accessors;
  while (<SRC>) {
    chop;
    if ($in_comment) {
      if (m@^/comment\b@) {
        &emit_comment(@comment_block);
        @comment_block = ();
        $in_comment = 0;
        }
      else {
        push(@comment_block, $_);
        }
      next;
      }
    if ($in_code) {
      if ($_ eq $end_marker) {
        $in_code = 0;
        }
      else {
        print TMP  $_, "\n";
        }
      next;
      }
    $line = $_;
    s/#.*//;   # Ignore comments
    s/^\s+//;  # Ignore leading  whitespace
    s/\s+$//;  # Ignore trailing whitespace
    next if ($_ eq '');
    if (substr($_, 0, 1) eq $cmd_pfx) {
      &parse_command;
      next;
      }
    &parse_field;
    }

  if (scalar(@properties) > 0) {
    &emit_accessors;
    }
  close(TMP);

  # Generate a nice proper header file by copying and modifying
  # prototype.h from this workspace.
  #
  # In place of the boiler-plate content, emit the block comments
  # and generated accessor macros.
  #
  $hdr_sym = '_SYS_' .  "\U${struct_name}" . '_H';
  $hdr_fname = $ENV{'SRC'} . '/prototypes/prototype.h';
  open(HDR, $hdr_fname) || die "open('${hdr_fname}') failed; $!\n";
  $peek = undef;
  while (<HDR>) {
    last if (m/Block comment/);
    if (defined($peek)) {
      print $peek;
      }
    s/_HEADER_H/${hdr_sym}/;
    $peek = $_;
    }
  &emit_prologue;
  while (<HDR>) {
    last if (m/#if.*__cplusplus/);
    }
  print $_;
  while (<HDR>) {
    print $_;
    last  if (m/^#endif/);
    }
  print "\n";
  print "#include <sys/bitfield32.h>\n";
  print "\n";
  open(TMP, $tmp_fname) || die "open('${tmp_fname}') failed; $!";
  while (<TMP>) {
    print $_;
    }
  close(TMP);
  while (<HDR>) {
    last  if (m/^void\sfunctions/);
    }
  while (<HDR>) {
    s/_HEADER_H/${hdr_sym}/;
    print $_;
    }
  close(HDR);
  }



sub emit_prologue {
  print
    "/*\n",
    " * This header file was generated from the following specifications:\n",
    " *\n";
  for (@specifications) {
    if (m@^\s*$@) {
      print " *\n";
      }
    else {
      print " * ", $_, "\n";
      }
    }
  print
    " *\n",
    " */\n",
    "\n";
  }

sub init_scan {
  $in_comment = 0;
  $in_code = 0;
  $end_marker = '';
  $size_in_bits = 64;
  $bit_index = -1;
  $direction = -1;
  $max_fld_size = 0;
  $reserved_instance = 0;
  @specifications = ();
  %readonly = ();
  $err_count = 0;
  @bfmt = ();
  }

sub init_accessors {
  @comment_block = ();
  @properties = ();
  }

sub parse_command {
  @cmdv = split;
  $cmd = $cmdv[0];
  $cmd =~ s/^.//;	# Strip off command prefix
  if ($cmd eq 'struct') {
    $struct_name = $cmdv[1];
    push(@specifications, ":struct ${struct_name}");
    }
  elsif ($cmd eq 'bits') {
    $size_in_bits = $cmdv[1];
    push(@specifications, sprintf('%sbits %d', $cmd_pfx, $size_in_bits));
    }
  elsif ($cmd eq 'readonly') {
    $modify = 0;
    push(@specifications, $cmd_pfx . $cmd);
    }
  elsif ($cmd eq 'comment') {
    $in_comment = 1;
    }
  elsif ($cmd eq 'code') {
    $in_code = 1;
    $end_marker = $cmdv[1];
    }
  elsif ($cmd eq 'emit') {
    if (scalar(@properties)) {
      &emit_accessors;
      }
    &init_accessors;
    }
  else {
    &emit_error("Unknown command '${cmd}'.");
    }
  }

sub parse_field {
  push(@specifications, $line);
  @fld = split;
  $nfields = scalar(@fld);
  if ($nfields != 2) {
    &emit_error("Expected 2 fields, got ${nfields}.");
    print STDERR
      " -> ", $_, "\n";
    next;
    }
  ($fld_name, $fld_spec) = @fld;
  if ($fld_spec =~ m/,/) {
    @fld_options = split(/,/, $fld_spec);
    $fld_size = shift(@fld_options);
    }
  else {
    $fld_size = $fld_spec;
    @fld_options = ();
    }
  if ($fld_size !~ m/^\d+$/) {
    &emit_error("Field ${fld_name} size must be numeric");
    }
  for $opt (@fld_options) {
    if ($opt eq 'ro') {
      $readonly{$fld_name} = 1;
      }
    else {
      &emit_error("Field ${fld_name}: unknown option, ${opt}\n");
      }
    }
  $fld_type = '';
  if ($fld_name eq 'reserved') {
    ++$reserved_instance;
    $fld_name = sprintf('rsv%d', $reserved_instance);
    $fld_type = 'r';
    }
  elsif ($fld_size > $max_fld_size) {
    $max_fld_size = $fld_size;
    }
  if ($bit_index == -1) {
    $bit_index = $size_in_bits - 1;
    }
  if ($direction == 1) {
    # Big endian bit order
    $nxt_index = $bit_index + $fld_size;
    $lsb = $bit_index;
    $msb = $nxt_index - 1;
    }
  else {
    # Little endian bit order
    $nxt_index = $bit_index - $fld_size;
    $msb = $bit_index;
    $lsb = $nxt_index + 1;
    }
  if ($msb >= $size_in_bits) {
    &emit_error("Field ${fld_name} exceeds ${size_in_bits} size.");
    }
  if ($lsb < 0) {
    &emit_error("Field ${fld_name}, lsb==${lsb}, must be >= 0.");
    }
  $shift = $lsb;
  if ($fld_name ne 'ignored') {
    push(@properties, join("\000", $fld_name, $fld_type, $fld_size, $msb, $lsb, $shift));
    }
  if ($fld_name ne 'reserved' && $fld_size == 1) {
    push(@bfmt, "\\" . sprintf('%o', $shift + 1) . "\U${fld_name}");
    }
  $bit_index = $nxt_index;
  }

sub emit_comment {
  print TMP  "/*\n";
  for (@_) {
    if (m@^\s*$@) {
      print TMP " *\n";
      }
    else {
      print TMP  ' * ', $_, "\n";
      }
    }
  print TMP  " */\n";
  }

sub emit_accessors {
  if ($verbose) {
    for (@properties) {
      print TMP  '  ', join(' ', split(/\000/, $_)), "\n";
      }
    }

  print TMP  "\n";

  @new_args = ();
  @new_flds = ();
  @get_exprs = ();
  @msk_exprs = ();
  @set_exprs = ();
  @resv_mask = ();
  $max_varname_length = 0;
  for (@properties) {
    ($fld_name, $fld_type, $fld_size, $msb, $lsb, $shift) = split(/\000/, $_);
    $size_var  = "\U${struct_name}_${fld_name}_SIZE";
    $shift_var = "\U${struct_name}_${fld_name}_SHIFT";
    $fld_mask_var  = "\U${struct_name}_${fld_name}_FLD_MASK";
    $val_mask_var  = "\U${struct_name}_${fld_name}_VAL_MASK";
    &make_var($size_var, sprintf('%u', $fld_size));
    &make_var($shift_var, sprintf('%u', $shift));
    $val_mask_n = (1 << $fld_size) - 1;
    $fld_mask_n = $val_mask_n << $shift;
    $val_mask_val = sprintf('0x%x', $val_mask_n);
    $fld_mask_val = sprintf('0x%x', $fld_mask_n);
    &make_var($fld_mask_var, $fld_mask_val);
    &make_var($val_mask_var, $val_mask_val);
    if ($fld_type eq '') {
      &make_field_accessors;
      }
    elsif ($fld_type eq 'r') {
      push(@resv_mask, $fld_mask_n);
      }
    else {
      die "INTERNAL ERROR: fld_type = '${fld_type}'\n";
      }
    }

  $tabwidth = 8 * int(($max_varname_length + 8) / 8);
  for (@vars) {
    ($definiens, $definiendum) = split(/\000/, $_);
    $tabs = ((($tabwidth - length($definiens)) + 7) / 8);
    $int_type = 'uint_' . $size_in_bits;
    if ($size_in_bits == $int_size) {
      $cast = '';
      }
    else {
      $cast = '(' . $int_type . ')';
      }
    print TMP
      "#define\t", $definiens, "\t" x $tabs, $cast, $definiendum, "\n";
    }

  if ($modify) {
    &emit_new_expr;
    }
  if (0) {
    &emit_msk_exprs;
    }
  &emit_get_exprs;
  if (scalar(@resv_mask)) {
    &emit_resv_expr;
    }
  if ($modify) {
    &emit_set_exprs;
    }
  if ($max_fld_size == 1) {
    &emit_bit_fmt;
    }
  }


sub make_var {
  local($sym, $val) = @_;
  local($symlen);

  $symlen = length($sym);
  if ($symlen > $max_varname_length) {
    $max_varname_length = $symlen;
    }
  push(@vars, join("\000", $sym, $val));
  }

sub make_field_accessors {
  push(@new_args, "\L${fld_name}");
  $fn = 'VALTOFLD' . $size_in_bits . 'C';
  $av = "(\L${fld_name}\E), ${shift}, ${fld_size}";
  push(@new_flds, "${fn}(${av})");
  push(@msk_exprs,
    join("\000",
      "\U${struct_name}_MSK_${fld_name}\E(var)",
      "((var)&${fld_mask_var})"
      )
    );

  $fn = 'EXTRACT' . $size_in_bits;
  $av = "(src), ${shift}, ${fld_size}";
  push(@get_exprs,
    join("\000",
      "\U${struct_name}_GET_${fld_name}\E(src)",
      "${fn}(${av})"
      )
    );

  $fn = 'DEPOSIT' . $size_in_bits;
  $av = "(src), (fld), ${shift}, ${fld_size}";
  push(@set_exprs,
    join("\000",
      $fld_name,
      "\U${struct_name}_SET_${fld_name}\E(src, fld)",
      "${fn}(${av})"
      )
    );
  }

sub emit_msk_exprs {
  print TMP
    "\n",
    "/*\n",
    " * Macros to mask individual fields in a ${struct_name}\n",
    " *\n",
    " */\n",
    "\n";
  for (@msk_exprs) {
    ($definiendum, $definiens) = split(/\000/, $_);
    print TMP  "#define\t${definiendum}\t\\\n\t${definiens}\n";
    }
  }

sub emit_get_exprs {
  print TMP
    "\n",
    "/*\n",
    " * Macros to get (extract) individual fields from a ${struct_name}\n",
    " *\n",
    " */\n",
    "\n";
  for (@get_exprs) {
    ($definiendum, $definiens) = split(/\000/, $_);
    print TMP  "#define\t${definiendum}\t\\\n\t${definiens}\n";
    }
  }

sub emit_resv_expr {
  $resv_mask = 0;
  for $resv_mask_n (@resv_mask) {
    $resv_mask |= $resv_mask_n;
    }
  $int_type = 'uint_' . $size_in_bits;
  if ($size_in_bits == $int_size) {
    $cast = '';
    }
  else {
    $cast = '(' . $int_type . ')';
    }
  print TMP
    "\n",
    "#define\t\U${struct_name}", '_RESV_MASK ',
        $cast, sprintf('0x%x', $resv_mask), "\n";
  }

sub emit_new_expr {
  local(@av);
  local($out);
  local($sep);

  print TMP
    "\n",
    "/*\n",
    " * Macro to construct a new ${struct_name}\n",
    " *\n",
    " */\n",
    "\n";
  @av = @new_args;
  $out = "#define\t" . "\U${struct_name}" . "_NEW(";
  $sep = '';
  while (scalar(@av) > 0) {
    # Compose lines with with tabs expanded to spaces, at first,
    # in order to make it easy to make decisions based on line length.
    # Then convert leading spaces to tabs.
    while (scalar(@av) > 0 && length($out . $sep . $av[0]) < 78) {
      if ($out ne '') {
        $out .= $sep . $av[0];
        }
      else {
        $out = "                " . $av[0];
        }
      shift @av;
      $sep = ', ';
      }
    $out =~ s/^                /\t\t/;
    if (scalar(@av) > 0) {
      print TMP $out, ", \\\n";
      }
    else {
      print TMP $out, ") \\\n";
      }
    $out = '';
    }
  
  # print TMP
  #   "#define\t" . "\U${struct_name}" . '_NEW', '(', join(', ', @new_args), ')', " \\\n";
  $sep = "\t(";
  for (@new_flds) {
    print TMP  $sep, $_;
    $sep = " \\\n\t| ";
    }
  print TMP  ")\n";
  }

sub emit_set_exprs {
  print TMP
    "\n",
    "/*\n",
    " * Macros to set individual fields of a ${struct_name}\n",
    " *\n",
    " */\n",
    "\n";
  for (@set_exprs) {
    ($fld_name, $definiendum, $definiens) = split(/\000/, $_);
    if (!defined($readonly{$fld_name})) {
      print TMP  "#define\t${definiendum}\t\\\n\t${definiens}\n";
      }
    }
  }

sub emit_error {
  for (@_) {
    print STDERR 'ERROR: ', $_, "\n";
    }
  for (@_) {
    print TMP  '#error ', $_, "\n";
    }
  ++$err_count;
  }

sub emit_bit_fmt {
  print TMP  "\n";
  print TMP  "#define\t", "\U${struct_name}", "_BFMT \"\\20\"";
  for (@bfmt) {
    print TMP  " \\\n";
    print TMP  "\t", "\"", $_, "\"";
    }
  print TMP  "\n";
  }
