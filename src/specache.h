/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __SPECACHE_HPP_
#define __SPECACHE_HPP_

#include "specs.h"

#include <string.h>
#include <stdlib.h>

class spec_directory_cache
{
  class filename_node
  {
    public :
    filename_node *left,*right,*next;
    char *fn;
    spec_directory *sd;
    char *filename() { return fn; }
    filename_node(char const *filename, spec_directory *dir)
    {
      fn = strdup(filename);
      sd = dir;
      next = left = right = 0;
    }
    // Both of these are the node's own: the name is strdup'd above and the
    // directory is new'd by the only caller. Without this the node was
    // freed and everything it owned stayed, which is where nearly all of
    // the 200 KB the sanitiser reported came from.
    ~filename_node() { free(fn); delete sd; }
    long size;
  } *fn_root,*fn_list;
  void clear(filename_node *f); // private recursive member
  long size;
  public :
  spec_directory *get_spec_directory(char const *filename, bFILE *fp=NULL);
  // fn_list too. It was left uninitialised and read on the first insert;
  // it worked only because the one instance of this class is a global, and
  // so started zeroed.
  spec_directory_cache() { fn_root=0; fn_list=0; size=0; }
  void clear();                             // frees up all allocated memory
  void load(bFILE *fp);
  void save(bFILE *fp);
  ~spec_directory_cache() { clear(); }
} ;

extern spec_directory_cache sd_cache;

#endif
