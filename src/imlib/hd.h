/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The HD override pack. Phase 6, block 6.6.
 *
 *  An image in data/hd/ replaces the one the game would have loaded from
 *  its .spe. The path is the one tools/spe-export writes, so an artist
 *  exports the art, works on a file, drops it back under the same name and
 *  the game picks it up.
 *
 *  **Same size as the original, and that is not a temporary limit.** The
 *  engine draws into an 8-bit buffer 320 pixels wide and every sprite is
 *  placed and clipped in those pixels: a sprite twice the size draws twice
 *  as big, not twice as sharp. Actual high-resolution art needs the
 *  renderer to composite sprites itself rather than blit them into that
 *  buffer, which is a larger job than this one and is written up in
 *  docs/plan/fase-06-visual.md.
 *
 *  What this is good for today is retouching: fixing a sprite, recolouring
 *  it, cleaning up a scan, and seeing it in the game without an editor.
 *
 *  Covers images, foreground and background tiles, and characters, which
 *  is every picture the cache hands out. Characters come through last
 *  because they are cut into a forward and a backward copy and the swap
 *  has to happen before either.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_IMLIB_HD_H_
#define ABUSE_IMLIB_HD_H_

class image;

namespace abuse::hd {

// Whether there is a data/hd/ at all. Checked once: without it nothing
// below costs anything, which is the usual case.
bool available();

// The file that would replace entry `name` of `spe_path`, if it exists.
// Returns an allocated string the caller owns, or null.
char *find(char const *spe_path, char const *name);

// Decodes it. Null when the file will not read, when it is not the size
// the game expects, or when the palette is not loaded yet; in every case
// the caller falls back to the original and says so once.
image *load(char const *path, int want_w, int want_h);

// Swaps `original` for the file at `path` when it reads and is the same
// size, and takes ownership of whichever it does not return. Null path or
// a refusal both give the original back, so a caller can write
//
//     im = abuse::hd::swap(path, im);
//
// and be done. The path is cleared when it is refused, so the refusal is
// reported once and not every time the entry is reloaded.
image *swap(char *&path, image *original);

// Off turns the whole thing off regardless of what is on disk.
bool enabled();
void set_enabled(bool on);

}

#endif
