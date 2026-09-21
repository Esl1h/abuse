#!/usr/bin/env bash
# Plays with the sanitisers on and everything written to a log.
#
# For chasing a crash that only happens while a person is playing. The
# sanitiser build is slower but it names the file and the line; the plain
# build only tells you the process died.
#
#   ./scripts/play-diagnostics.sh              # sanitiser build
#   ./scripts/play-diagnostics.sh release      # plain build, still logged
#
# The log goes to /tmp/abuse-<build>-<timestamp>.log and the path is
# printed at the end. Send that file.

set -uo pipefail

build=${1:-asan}
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
bin="$root/build/$build/src/abuse"

[ -x "$bin" ] || {
    echo "no binary at $bin"
    echo "build it with: cmake --build --preset $build"
    exit 1
}

log="/tmp/abuse-$build-$(date +%Y%m%d-%H%M%S).log"

# Leak detection off: the engine abandons memory at exit by design and the
# report at the end would bury whatever the crash said. Halting on the
# first error is what gives a usable trace, and a stack from the
# undefined-behaviour checks as well, which is the half that pointed at the
# particle code last time.
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=0"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"

# Unbuffered, or the last lines before a crash never reach the file.
export ABUSE_UNBUFFERED=1

echo "log: $log"
echo

cd "$root" || exit 1
"$bin" -datadir ./data -window 2>&1 | tee "$log"
status=${PIPESTATUS[0]}

echo
echo "-------------------------------------------------------------"
if grep -qE "ERROR: AddressSanitizer|runtime error" "$log"; then
    echo "The sanitiser said something. Send this file:"
elif [ "$status" -ne 0 ]; then
    echo "It exited with $status. Send this file:"
else
    echo "Clean exit. The log is here anyway:"
fi
echo "  $log"
