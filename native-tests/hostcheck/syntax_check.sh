#!/usr/bin/env bash
# Best-effort host syntax check for the Android-specific engine sources.
#
# These files (#include <jni.h>, <android/log.h>, <oboe/Oboe.h>) cannot be
# built without the NDK + Oboe, so they are not covered by run_tests.sh. This
# script type-checks them with -fsyntax-only using the JDK's real jni.h plus the
# minimal stubs in this directory, catching typos, signature mismatches, and
# bad member access without a full Android toolchain.
#
# -fpermissive is required for one line: the desktop JDK declares
# JavaVM::AttachCurrentThread(void**, void*) while the Android NDK declares it
# (JNIEnv**, void*). The code uses the Android-correct JNIEnv** form, so the
# host build only warns about it.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SRC="${REPO_ROOT}/audio/src/main/cpp"

JAVA_INCLUDE="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}/include"
if [[ ! -f "${JAVA_INCLUDE}/jni.h" ]]; then
    echo "error: jni.h not found under ${JAVA_INCLUDE}; set JAVA_HOME" >&2
    exit 1
fi

CXX="${CXX:-g++}"
FILES=(JniSupport.cpp AudioRecordReader.cpp OutputStream.cpp AudioEngine.cpp jni_bridge.cpp)

status=0
for f in "${FILES[@]}"; do
    if "${CXX}" -std=c++17 -fsyntax-only -fpermissive -Wall -Wextra \
        -I"${SRC}" -I"${SCRIPT_DIR}" \
        -I"${JAVA_INCLUDE}" -I"${JAVA_INCLUDE}/linux" \
        "${SRC}/${f}" 2>/tmp/hostcheck_${f}.log; then
        echo "[ OK ] ${f}"
    else
        echo "[FAIL] ${f}"
        cat /tmp/hostcheck_${f}.log
        status=1
    fi
done
exit "${status}"
