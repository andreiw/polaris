// Copyright (c) 1994, 1995 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)CatalogEntry.h	1.4	00/07/17 SMI"

#ifndef CatalogEntry_INCLUDED
#define CatalogEntry_INCLUDED 1

#include "Location.h"
#include "StringC.h"
#include <stddef.h>

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

struct CatalogEntry {
  StringC to;
  Location loc;
  size_t catalogNumber;
  size_t baseNumber;
  size_t serial;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not CatalogEntry_INCLUDED */
