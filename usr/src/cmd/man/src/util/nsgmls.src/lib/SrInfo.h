// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)SrInfo.h	1.4	00/07/17 SMI"

#ifndef SrInfo_INCLUDED
#define SrInfo_INCLUDED

#include "types.h"
#include "StringOf.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

struct SrInfo {
  String<EquivCode> chars;
  int bSequenceLength;
  String<EquivCode> chars2;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not SrInfo_INCLUDED */
