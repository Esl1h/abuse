# Applies one patch to a dependency CPM fetched, and does nothing when it is
# already applied.
#
# CPM's own PATCHES keyword needs the `patch` program, which is not installed
# everywhere and is not on the reference host. git is, because the dependency
# arrived through it, and `git apply --reverse --check` answers "already
# applied" without changing anything, which is what makes this safe to run
# again on a tree that has been patched.
#
# Invoked as:
#   cmake -DGIT=<git> -DPATCH=<file> -P apply.cmake
# from the dependency's source directory, which is where FetchContent runs a
# PATCH_COMMAND.

if(NOT GIT OR NOT PATCH)
    message(FATAL_ERROR "apply.cmake needs -DGIT= and -DPATCH=")
endif()

execute_process(
    COMMAND "${GIT}" apply --reverse --check "${PATCH}"
    RESULT_VARIABLE already_applied
    OUTPUT_QUIET ERROR_QUIET)

if(already_applied EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH}")
    return()
endif()

execute_process(
    COMMAND "${GIT}" apply "${PATCH}"
    RESULT_VARIABLE failed
    ERROR_VARIABLE why)

if(NOT failed EQUAL 0)
    message(FATAL_ERROR "Could not apply ${PATCH}:\n${why}")
endif()

message(STATUS "Applied ${PATCH}")
