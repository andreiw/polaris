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
 * ident	"@(#)OptionContext.java	1.3	05/06/08 SMI"
 *
 * Copyright 2001-2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

package com.sun.dhcpmgr.data;

import java.io.Serializable;

/**
 * OptionContext simply defines the attributes that should be associated
 * with an Option context. Simply a container.
 */
public class OptionContext implements Serializable {

    private byte code;
    private String dhcptabString;
    private String string;

    // Serialization id for this class
    static final long serialVersionUID = -8656392905082090762L;

    /**
     * Constructs a fully defined instance of an OptionContext.
     * @param code the context code
     * @param dhcptabString the dhcptab string definition for the context
     * @param msgid the msgid for the description of the context
     */
    public OptionContext(byte code, String dhcptabString, String msgid) {
	this.code = code;
	this.dhcptabString = dhcptabString;
	this.string = ResourceStrings.getString(msgid);
    } // constructor

    /**
     * Returns the code for the context
     * @returns the code for the context
     */
    public byte getCode() {
	return code;
    } // getCode

    /**
     * Returns the dhcptab string definition for the context
     * @returns the dhcptab string definition for the context
     */
    public String getDhcptabString() {
	return dhcptabString;
    } // getDhcptabString

    /**
     * Returns a string representation of this object.
     * @return a string representation of this object.
     */
    public String toString() {
	return (string);
    } // toString

} // OptionContext
