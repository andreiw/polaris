// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)StringC.h	1.4	00/07/17 SMI"

#ifndef StringC_INCLUDED
#define StringC_INCLUDED 1

#include "types.h"
#include "StringOf.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

typedef String<Char> StringC;

#ifdef SP_NAMESPACE
}
#endif

#endif /* not StringC_INCLUDED */
