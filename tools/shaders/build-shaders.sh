#!/usr/bin/env bash
#
# Compiles the SDL_GPU shaders to SPIR-V and writes the result next to the
# sources, where it is committed.
#
# The blobs are in the repository on purpose. SDL_GPU wants shaders in each
# backend's own format, and producing them needs a toolchain per platform:
# SPIR-V for Vulkan, DXIL for D3D12, a metallib for Metal. Requiring all
# three at build time would mean the game could not be built without the
# Vulkan SDK, the Windows SDK and a Mac. Committing them means this script
# runs when a shader changes and never otherwise.
#
#   ./tools/shaders/build-shaders.sh
#
# Only SPIR-V so far, so only the Vulkan backend of SDL_GPU can be used.
# DXIL and MSL need machines this has not run on; see docs/plan/fase-06.

set -euo pipefail

here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd -- "$here/../.." && pwd)
src="$root/src/render/gpu"

compile() {
    local kind=$1 in=$2 out=$3
    if command -v glslc >/dev/null 2>&1; then
        glslc -fshader-stage="$kind" -o "$out" "$in"
        echo "$(basename "$out"): $(stat -c%s "$out") bytes (glslc)"
        return
    fi

    # No glslc. Fedora ships libshaderc_shared without the binary, so build
    # the small wrapper beside this script and use the library directly.
    local helper="$root/build/glsl2spv"
    mkdir -p "$root/build"
    if [ ! -x "$helper" ] || [ "$here/glsl2spv.c" -nt "$helper" ]; then
        cc -O2 -o "$helper" "$here/glsl2spv.c" -ldl
    fi
    "$helper" "$kind" "$in" "$out"
}

compile vert "$src/palette.vert.glsl" "$src/palette.vert.spv"
compile frag "$src/palette.frag.glsl" "$src/palette.frag.spv"

echo
echo "Committed as they are. Rerun this after changing a .glsl."
