// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)SJISCodingSystem.h	1.4	00/07/17 SMI"

#ifndef SJISCodingSystem_INCLUDED
#define SJISCodingSystem_INCLUDED 1

#include "CodingSystem.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class SP_API SJISCodingSystem : public CodingSystem {
public:
  Decoder *makeDecoder() const;
  Encoder *makeEncoder() const;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not SJISCodingSystem_INCLUDED */
