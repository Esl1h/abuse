/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See compat.h.
 *
 *  This software was released into the Public Domain.
 */

#include "compat.h"

#include <errno.h>

#if !defined(_MSC_VER)
#   include <sys/types.h>
#endif

namespace abuse {

bool make_directory(char const *path)
{
#if defined(_MSC_VER)
    int rc = _mkdir(path);
#else
    // 0755: the owner writes, everyone else may enter and read. The same
    // permissions a shell's mkdir gives under the default umask.
    int rc = mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR
                         | S_IRGRP | S_IXGRP
                         | S_IROTH | S_IXOTH);
#endif
    return rc == 0 || errno == EEXIST;
}

}
