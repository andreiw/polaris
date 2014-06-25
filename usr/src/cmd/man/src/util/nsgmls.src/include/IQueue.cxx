// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.
#pragma ident	"@(#)IQueue.cxx	1.4	00/07/17 SMI"

#ifndef IQueue_DEF_INCLUDED
#define IQueue_DEF_INCLUDED 1

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

template<class T>
void IQueue<T>::clear()
{
  while (!empty())
    delete get();
}

#ifdef SP_NAMESPACE
}
#endif

#endif /* not IQueue_DEF_INCLUDED */
