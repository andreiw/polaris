// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)TokenMessageArg.h	1.4	00/07/17 SMI"

#ifndef TokenMessageArg_INCLUDED
#define TokenMessageArg_INCLUDED 1
#ifdef __GNUG__
#pragma interface
#endif

#include "MessageArg.h"
#include "types.h"
#include "Mode.h"
#include "Syntax.h"
#include "Sd.h"
#include "Ptr.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class TokenMessageArg : public MessageArg {
public:
  TokenMessageArg(Token token, Mode mode,
		  const ConstPtr<Syntax> &syntax,
		  const ConstPtr<Sd> &sd);
  MessageArg *copy() const;
  void append(MessageBuilder &) const;
private:
  Token token_;
  Mode mode_;
  ConstPtr<Syntax> syntax_;
  ConstPtr<Sd> sd_;
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not TokenMessageArg_INCLUDED */
