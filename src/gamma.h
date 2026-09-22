/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __GAMMA_HPP_
#define __GAMMA_HPP_

#include "palette.h"
void gamma_correct(palette *&pal, int force_menu=0);

// The stored calibration, and setting it without the 1995 picker.
//
// 16 is the reference: gamma exactly 1, the palette untouched, and the
// value every golden frame was recorded with. Below it the picture is
// darker, above it brighter; 1 is gamma 2, which halves a mid grey.
//
// set_gamma_value rebuilds the palette and loads it, so the change is
// visible at once, and writes gamma.lsp, which is where this has always
// been kept. Out of range values are clamped to 1..128, as the picker's
// were.
int gamma_value();
void set_gamma_value(int dg);

#endif
