// Copyright (c) 1997 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)Big5CodingSystem.h	1.3	00/07/17 SMI"

#ifndef Big5CodingSystem_INCLUDED
#define Big5CodingSystem_INCLUDED 1

#include "CodingSystem.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class SP_API Big5CodingSystem : public CodingSystem {
public:
  Decoder *makeDecoder() const;
  Encoder *makeEncoder() const;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not Big5CodingSystem_INCLUDED */
