// Copyright (c) 1996 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)StringVectorMessageArg.cxx	1.4	00/07/17 SMI"

#ifdef __GNUG__
#pragma implementation
#endif
#include "splib.h"
#include "StringVectorMessageArg.h"
#include "MessageBuilder.h"
#include "ParserMessages.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

StringVectorMessageArg::StringVectorMessageArg(const Vector<StringC> &v)
: v_(v)
{
}

MessageArg *StringVectorMessageArg::copy() const
{
  return new StringVectorMessageArg(*this);
}

void StringVectorMessageArg::append(MessageBuilder &builder) const
{
  for (size_t i = 0; i < v_.size(); i++) {
    if (i > 0)
      builder.appendFragment(ParserMessages::listSep);
    builder.appendChars(v_[i].data(), v_[i].size());
  }
}

#ifdef SP_NAMESPACE
}
#endif
