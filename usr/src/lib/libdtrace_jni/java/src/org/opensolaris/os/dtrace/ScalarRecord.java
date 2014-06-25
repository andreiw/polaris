/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * ident	"@(#)ScalarRecord.java	1.1	06/02/16 SMI"
 */
package org.opensolaris.os.dtrace;

import java.io.*;
import java.util.Arrays;
import java.beans.*;

/**
 * A traced D primitive generated by a DTrace action such as {@code
 * trace()} or {@code tracemem()}, or else an element in a composite
 * value generated by DTrace.
 * <p>
 * Immutable.  Supports persistence using {@link java.beans.XMLEncoder}.
 *
 * @author Tom Erickson
 */
public final class ScalarRecord implements ValueRecord, Serializable {
    static final long serialVersionUID = -34046471695050108L;
    static final int RAW_BYTES_INDENT = 5;

    static {
	try {
	    BeanInfo info = Introspector.getBeanInfo(ScalarRecord.class);
	    PersistenceDelegate persistenceDelegate =
		    new DefaultPersistenceDelegate(
		    new String[] {"value"})
	    {
		/*
		 * Need to prevent DefaultPersistenceDelegate from using
		 * overridden equals() method, resulting in a
		 * StackOverFlowError.  Revert to PersistenceDelegate
		 * implementation.  See
		 * http://forum.java.sun.com/thread.jspa?threadID=
		 * 477019&tstart=135
		 */
		protected boolean
		mutatesTo(Object oldInstance, Object newInstance)
		{
		    return (newInstance != null && oldInstance != null &&
			    oldInstance.getClass() == newInstance.getClass());
		}
	    };
	    BeanDescriptor d = info.getBeanDescriptor();
	    d.setValue("persistenceDelegate", persistenceDelegate);
	} catch (IntrospectionException e) {
	    System.out.println(e);
	}
    }

    /** @serial */
    private final Object value;

    /**
     * Creates a scalar record with the given DTrace primitive.
     *
     * @param v DTrace primitive data value
     * @throws NullPointerException if the given value is null
     * @throws ClassCastException if the given value is not a DTrace
     * primitive type listed as a possible return value of {@link
     * #getValue()}
     */
    public
    ScalarRecord(Object v)
    {
	value = v;
	validate();
    }

    private void
    validate()
    {
	if (value == null) {
	    throw new NullPointerException();
	}
	// Short-circuit-evaluate common cases first
	if (!((value instanceof Number) ||
		(value instanceof String) ||
		(value instanceof byte[]))) {
	    throw new ClassCastException("value is not a D primitive");
        }
    }

    /**
     * Gets the traced D primitive value of this record.
     *
     * @return a non-null value whose type is one of the following:
     * <ul>
     * <li>{@link Number}</li>
     * <li>{@link String}</li>
     * <li>byte[]</li>
     * </ul>
     */
    public Object
    getValue()
    {
	return value;
    }

    /**
     * Compares the specified object with this record for equality.
     * Defines equality as having the same value.
     *
     * @return {@code true} if and only if the specified object is also
     * a {@code ScalarRecord} and the values returned by the {@link
     * #getValue()} methods of both instances are equal, {@code false}
     * otherwise.  Values are compared using {@link
     * java.lang.Object#equals(Object o) Object.equals()}, unless they
     * are arrays of raw bytes, in which case they are compared using
     * {@link java.util.Arrays#equals(byte[] a, byte[] a2)}.
     */
    @Override
    public boolean
    equals(Object o)
    {
	if (o instanceof ScalarRecord) {
	    ScalarRecord r = (ScalarRecord)o;
	    if (value instanceof byte[]) {
		if (r.value instanceof byte[]) {
		    byte[] a1 = (byte[])value;
		    byte[] a2 = (byte[])r.value;
		    return Arrays.equals(a1, a2);
		}
		return false;
	    }
	    return value.equals(r.value);
	}
	return false;
    }

    /**
     * Overridden to ensure that equal instances have equal hashcodes.
     *
     * @return {@link java.lang.Object#hashCode()} of {@link
     * #getValue()}, or {@link java.util.Arrays#hashCode(byte[] a)} if
     * the value is a raw byte array
     */
    @Override
    public int
    hashCode()
    {
	if (value instanceof byte[]) {
	    return Arrays.hashCode((byte[])value);
	}
	return value.hashCode();
    }

    private static final int BYTE_SIGN_BIT = 1 << 7;

    /**
     * Static utility for treating a byte as unsigned by converting it
     * to int without sign extending.
     */
    static int
    unsignedByte(byte b)
    {
        if (b < 0) {
	    b ^= (byte)BYTE_SIGN_BIT;
	    return ((int)b) | BYTE_SIGN_BIT;
	}
	return (int)b;
    }

    static String
    hexString(int n, int width)
    {
	String s = Integer.toHexString(n);
	int len = s.length();
	if (width < len) {
	    s = s.substring(len - width);
	} else if (width > len) {
	    s = (spaces(width - len) + s);
	}
	return s;
    }

    static String
    spaces(int n)
    {
	StringBuffer buf = new StringBuffer();
	for (int i = 0; i < n; ++i) {
	    buf.append(' ');
	}
	return buf.toString();
    }

    /**
     * Represents a byte array as a table of unsigned byte values in hex,
     * 16 per row ending in their corresponding character
     * representations (or a period (&#46;) for each unprintable
     * character).  Uses default indentation.
     *
     * @see ScalarRecord#rawBytesString(byte[] bytes, int indent)
     */
    static String
    rawBytesString(byte[] bytes)
    {
	return rawBytesString(bytes, RAW_BYTES_INDENT);
    }

    /**
     * Represents a byte array as a table of unsigned byte values in hex,
     * 16 per row ending in their corresponding character
     * representations (or a period (&#46;) for each unprintable
     * character).  The table begins and ends with a newline, includes a
     * header row, and uses a newline at the end of each row.
     *
     * @param bytes array of raw bytes treated as unsigned when
     * converted to hex display
     * @param indent number of spaces to indent each line of the
     * returned string
     * @return table representation of 16 bytes per row as hex and
     * character values
     */
    static String
    rawBytesString(byte[] bytes, int indent)
    {
	// ported from libdtrace/common/dt_consume.c dt_print_bytes()
	int i, j;
	int u;
	StringBuffer buf = new StringBuffer();
	String leftMargin = spaces(indent);
	buf.append('\n');
	buf.append(leftMargin);
	buf.append("      ");
	for (i = 0; i < 16; i++) {
	    buf.append("  ");
	    buf.append("0123456789abcdef".charAt(i));
	}
	buf.append("  0123456789abcdef\n");
	int nbytes = bytes.length;
	String hex;
	for (i = 0; i < nbytes; i += 16) {
	    buf.append(leftMargin);
	    buf.append(hexString(i, 5));
	    buf.append(':');

	    for (j = i; (j < (i + 16)) && (j < nbytes); ++j) {
		buf.append(hexString(unsignedByte(bytes[j]), 3));
	    }

	    while ((j++ % 16) != 0) {
		buf.append("   ");
	    }

	    buf.append("  ");

	    for (j = i; (j < (i + 16)) && (j < nbytes); ++j) {
		u = unsignedByte(bytes[j]);
		if ((u < ' ') || (u > '~')) {
		    buf.append('.');
		} else {
		    buf.append((char) u);
		}
	    }

	    buf.append('\n');
	}

	return buf.toString();
    }

    static String
    valueToString(Object value)
    {
	String s;
	if (value instanceof byte[]) {
	    s = rawBytesString((byte[])value);
	} else {
	    s = value.toString();
	}
	return s;
    }

    private void
    readObject(ObjectInputStream s)
            throws IOException, ClassNotFoundException
    {
	s.defaultReadObject();
	// check class invariants
	try {
	    validate();
	} catch (Exception e) {
	    throw new InvalidObjectException(e.getMessage());
	}
    }

    /**
     * Gets the natural string representation of the traced D primitive.
     *
     * @return the value of {@link Object#toString} when called on
     * {@link #getValue()}; or if the value is an array of raw bytes, a
     * table displaying 16 bytes per row in unsigned hex followed by the
     * ASCII character representations of those bytes (each unprintable
     * character is represented by a period (.))
     */
    public String
    toString()
    {
	return ScalarRecord.valueToString(getValue());
    }
}
