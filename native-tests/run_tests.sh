#!/usr/bin/env bash
# Builds and runs the host-side native unit tests for the :audio engine.
#
# These cover the Android-independent real-time primitives (lock-free ring
# buffer, PCM format conversion). The ring buffer is additionally exercised
# under ThreadSanitizer to catch data races in the lock-free fast path.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# ThreadSanitizer needs a compiler whose sanitizer runtime is actually
# installed. GCC ships libtsan in its own package; prefer it when present.
CXX="${CXX:-}"
ENABLE_TSAN=ON
if [[ -z "${CXX}" ]]; then
    # Note: plain `grep ... >/dev/null` (not `grep -q`) so grep reads all of
    # ldconfig's output. `grep -q` exits early, which under `set -o pipefail`
    # makes ldconfig fail with SIGPIPE and falsely reports libtsan as missing.
    if command -v g++ >/dev/null && ldconfig -p 2>/dev/null | grep libtsan >/dev/null; then
        CXX=g++
    elif command -v clang++ >/dev/null; then
        CXX=clang++
        ENABLE_TSAN=OFF
        echo "warning: using clang++ without ThreadSanitizer (libclang_rt.tsan not found)"
    else
        CXX=c++
        ENABLE_TSAN=OFF
    fi
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TSAN="${ENABLE_TSAN}" >/dev/null

cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 2)"

ctest --test-dir "${BUILD_DIR}" --output-on-failure
