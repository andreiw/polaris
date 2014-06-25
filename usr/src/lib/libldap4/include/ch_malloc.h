/*
 *
 * Copyright 07/17/00 Sun Microsystems, Inc. All Rights Reserved
 *
 */

#pragma ident	"@(#)ch_malloc.h	1.2	00/07/17 SMI"

char * ch_malloc( unsigned long size );
char * ch_realloc( char *block, unsigned long size );
char * ch_calloc( unsigned long nelem, unsigned long size );
char * ch_strdup( char *s1 );
void ch_free(void *ptr);
