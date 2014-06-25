/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kifulprv.h	1.1	00/06/27 SMI"

/* Copyright (C) RSA Data Security, Inc. created 1990, 1996.  This is an
   unpublished work protected as such under copyright law.  This work
   contains proprietary, confidential, and trade secret information of
   RSA Data Security, Inc.  Use, disclosure or reproduction without the
   express written authorization of RSA Data Security, Inc. is
   prohibited.
 */

int CacheFullPrivateKey PROTO_LIST
  ((B_Key *, ITEM *, ITEM *, ITEM *, ITEM *, ITEM *, ITEM *));
int GetFullPrivateKeyInfo PROTO_LIST
  ((ITEM *, ITEM *, ITEM *, ITEM *, ITEM *, ITEM *, B_Key *));
