// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)MessageBuilder.h	1.4	00/07/17 SMI"

#ifndef MessageBuilder_INCLUDED
#define MessageBuilder_INCLUDED 1

#include <stddef.h>
#include "types.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class OtherMessageArg;
class MessageFragment;

class SP_API MessageBuilder {
public:
  virtual void appendNumber(unsigned long) = 0;
  virtual void appendOrdinal(unsigned long) = 0;
  virtual void appendChars(const Char *, size_t) = 0;
  virtual void appendOther(const OtherMessageArg *) = 0;
  virtual void appendFragment(const MessageFragment &) = 0;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not MessageBuilder_INCLUDED */
