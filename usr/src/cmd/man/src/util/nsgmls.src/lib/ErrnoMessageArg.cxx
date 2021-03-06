// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)ErrnoMessageArg.cxx	1.4	00/07/17 SMI"

#include "splib.h"
#include "ErrnoMessageArg.h"
#include "StringOf.h"
#include "MessageBuilder.h"

#include <string.h>

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

RTTI_DEF1(ErrnoMessageArg, OtherMessageArg)

MessageArg *ErrnoMessageArg::copy() const
{
  return new ErrnoMessageArg(*this);
}

#ifdef SP_NAMESPACE
}
#endif
