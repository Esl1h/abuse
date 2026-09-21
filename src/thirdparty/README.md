# Third-party sources

Vendored, not fetched. Each of these is a single file with no build system
of its own, and pinning a copy is less machinery than a package for one
header.

## stb_image.h

- **Version**: v2.30
- **Origin**: <https://github.com/nothings/stb>
- **Licence**: public domain, or MIT at the user's choice. Both are
  compatible with the GPL-2.0-or-later this project ships under.
- **Used for**: decoding the PNGs of the HD override pack (phase 6, block
  6.6). SDL3 on its own reads only BMP, and the alternative was adding
  SDL3_image as a dependency for one decoder.
- **Compiled in**: `src/imlib/hd.cpp`, which is the only place that defines
  `STB_IMAGE_IMPLEMENTATION`. Only the PNG decoder is built
  (`STBI_ONLY_PNG`), because that is the only format the pack uses.

Updating it means replacing the file and rerunning the suite. It has no
configuration beyond the defines in `hd.cpp`.
