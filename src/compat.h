/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The handful of POSIX names MSVC does not provide.
 *
 *  Include this instead of <strings.h>, and instead of reaching for S_ISDIR
 *  or the S_I* mode bits directly. Everything here is a name the C library
 *  has under a different spelling on Windows; nothing here changes behaviour
 *  on a platform that already has the POSIX name.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_COMPAT_H_
#define ABUSE_COMPAT_H_

#include <sys/stat.h>

#if defined(_MSC_VER)

#   include <direct.h>
#   include <string.h>

#   define strcasecmp  _stricmp
#   define strncasecmp _strnicmp
#   define getcwd      _getcwd

#   ifndef S_ISDIR
#       define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#   endif
#   ifndef S_ISREG
#       define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#   endif

#else

#   include <strings.h>
#   include <unistd.h>

#endif

namespace abuse {

// M_PI is a POSIX extension. MSVC only defines it when _USE_MATH_DEFINES is
// set before every <math.h>, which is a rule the next person to add an
// include will not know about.
constexpr double kPi = 3.14159265358979323846;

// Creates one directory, and says nothing when it is already there. The mode
// argument does not exist on Windows, where a directory inherits its
// permissions, so it is not part of the signature.
//
// Returns false only for a real failure, which a caller may want to report.
bool make_directory(char const *path);

}

#endif
