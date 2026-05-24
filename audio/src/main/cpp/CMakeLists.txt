cmake_minimum_required(VERSION 3.22.1)
project(vocalremover_audio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vocalremover_audio SHARED
    jni_bridge.cpp
    JniSupport.cpp
    AudioEngine.cpp
    AudioRecordReader.cpp
    OutputStream.cpp)

# Oboe is pulled in as a prefab package from the com.google.oboe:oboe AAR
# (enabled via android.buildFeatures.prefab in the module build.gradle.kts).
find_package(oboe REQUIRED CONFIG)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(vocalremover_audio
    oboe::oboe
    ${log-lib}
    ${android-lib})

target_compile_options(vocalremover_audio PRIVATE
    -Wall -Wextra -ffast-math -fno-finite-math-only)
