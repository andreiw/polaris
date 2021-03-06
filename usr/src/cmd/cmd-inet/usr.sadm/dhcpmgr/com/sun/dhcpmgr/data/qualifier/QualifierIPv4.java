/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * ident	"@(#)QualifierIPv4.java	1.2	05/06/08 SMI"
 *
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

package com.sun.dhcpmgr.data.qualifier;

import java.util.regex.*;

/**
 * An implementation of the qualifier type that provides a string type
 * where values must be a valid IPv4 address.
 */
public class QualifierIPv4 extends QualifierString {

    private static final String octetRegex =
				    "0*(1?[0-9]{1,2}|2([0-4][0-9]|5[0-5]))";

    private static final String addressRegex =
				    "(" + octetRegex + "[.]){3}" + octetRegex;

    private static Pattern pattern;

    public Object parseValue(String value) {
	if (value == null) {
	    return null;
	}

	value = value.trim();

	Matcher matcher = pattern.matcher(value);

	return (matcher.matches() ? value : null);
    }

    static {
	pattern = Pattern.compile(addressRegex);
    }
}
