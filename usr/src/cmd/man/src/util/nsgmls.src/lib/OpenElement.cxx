// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)OpenElement.cxx	1.4	00/07/17 SMI"

#ifdef __GNUG__
#pragma implementation
#endif
#include "splib.h"
#include "OpenElement.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

OpenElement::OpenElement(const ElementType *type,
			 Boolean net,
			 Boolean included,
			 const ShortReferenceMap *map,
			 const Location &startLocation)
: elementType_(type),
  netEnabling_(net),
  included_(included),
  matchState_(type->definition()->compiledModelGroup()),
  map_(map),
  startLocation_(startLocation),
  declaredContent_(type->definition()->declaredContent())
{
}

#ifdef SP_NAMESPACE
}
#endif