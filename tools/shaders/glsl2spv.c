/*
 *  Compiles GLSL to SPIR-V for the SDL_GPU path.
 *
 *  A fallback, not the normal route. Where glslc is installed (the Vulkan
 *  SDK, or the shaderc package on most distributions) build-shaders.sh uses
 *  that and never builds this. Fedora ships libshaderc_shared without the
 *  binary and without headers, which is where this came from: the shaderc C
 *  API is stable, so the handful of entry points it needs are declared here
 *  and the library is opened at run time.
 *
 *  Not part of the game build. It runs when someone regenerates the shader
 *  blobs, and the blobs are committed so that nobody else needs it.
 *
 *      cc -o glsl2spv glsl2spv.c -ldl
 *      ./glsl2spv vert in.glsl out.spv
 *
 *  This software was released into the Public Domain.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From shaderc/shaderc.h, which is not installed here. */
typedef void *shaderc_compiler_t;
typedef void *shaderc_compile_options_t;
typedef void *shaderc_compilation_result_t;

enum { kVertex = 0, kFragment = 1 };
enum { kSuccess = 0 };

static shaderc_compiler_t (*compiler_initialize)(void);
static shaderc_compilation_result_t (*compile_into_spv)(
    shaderc_compiler_t, char const *, size_t, int, char const *,
    char const *, shaderc_compile_options_t);
static size_t (*result_get_length)(shaderc_compilation_result_t);
static char const *(*result_get_bytes)(shaderc_compilation_result_t);
static int (*result_get_status)(shaderc_compilation_result_t);
static char const *(*result_get_error)(shaderc_compilation_result_t);

static void *need(void *lib, char const *name)
{
    void *sym = dlsym(lib, name);
    if (!sym)
    {
        fprintf(stderr, "libshaderc has no %s\n", name);
        exit(1);
    }
    return sym;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <vert|frag> <in.glsl> <out.spv>\n", argv[0]);
        return 2;
    }

    int kind = strcmp(argv[1], "vert") == 0 ? kVertex : kFragment;

    void *lib = dlopen("libshaderc_shared.so.1", RTLD_NOW);
    if (!lib)
        lib = dlopen("libshaderc_shared.so", RTLD_NOW);
    if (!lib)
    {
        fprintf(stderr, "cannot open libshaderc_shared: %s\n", dlerror());
        return 1;
    }

    compiler_initialize = need(lib, "shaderc_compiler_initialize");
    compile_into_spv = need(lib, "shaderc_compile_into_spv");
    result_get_length = need(lib, "shaderc_result_get_length");
    result_get_bytes = need(lib, "shaderc_result_get_bytes");
    result_get_status = need(lib, "shaderc_result_get_compilation_status");
    result_get_error = need(lib, "shaderc_result_get_error_message");

    FILE *in = fopen(argv[2], "rb");
    if (!in) { perror(argv[2]); return 1; }
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    char *src = malloc(size + 1);
    if (fread(src, 1, size, in) != (size_t)size) { perror("read"); return 1; }
    src[size] = 0;
    fclose(in);

    shaderc_compilation_result_t r = compile_into_spv(
        compiler_initialize(), src, size, kind, argv[2], "main", NULL);

    if (result_get_status(r) != kSuccess)
    {
        fprintf(stderr, "%s", result_get_error(r));
        return 1;
    }

    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror(argv[3]); return 1; }
    fwrite(result_get_bytes(r), 1, result_get_length(r), out);
    fclose(out);

    printf("%s: %zu bytes of SPIR-V\n", argv[3], result_get_length(r));
    return 0;
}
