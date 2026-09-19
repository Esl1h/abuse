/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#include "stack.h"
#ifndef __LISP_GC_HPP_
#define __LISP_GC_HPP_

// Stack user progs can push data and have it GCed
extern GrowStack<void> l_user_stack;

// This pointer reference stack lists all pointers to temporary lisp
// objects. This allows the pointers to be automatically modified if an
// object allocation triggers a garbage collection.
class PtrRef
{
public:
    template<typename T> inline PtrRef(T *&ref)
    {
        stack.push((void **)&ref);
    }

    template<typename T> inline PtrRef(T * const &ref)
    {
        stack.push((void **)&ref);
    }

    // Registering an rvalue stores the address of a temporary that dies at the
    // end of the declaration, leaving the collector writing to dead stack.
    // `PtrRef ref(this)` used to compile and do exactly that.
    template<typename T> PtrRef(T *&&) = delete;

    inline ~PtrRef()
    {
        stack.pop(1);
    }

    // Stack of user pointers, user pointers get remapped on GC
    static GrowStack<void *> stack;
};

#endif

