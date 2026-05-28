cmake_minimum_required(VERSION 3.22.1)
project(vocalremover_audio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_SOURCES
    jni_bridge.cpp
    JniSupport.cpp
    AudioEngine.cpp
    AudioRecordReader.cpp
    OutputStream.cpp
    ProcessingThread.cpp
    EngineRegistry.cpp
    SpectrogramProcessor.cpp
    StereoSpectrogramProcessor.cpp)

# ONNX Runtime is optional: Gradle extracts the AAR's headers and per-ABI
# libonnxruntime.so into ORT_HOME and passes it here. When present, the ONNX
# separator is compiled and linked; otherwise the engine builds passthrough +
# spectrogram-only and the app falls back at runtime.
set(HAVE_ORT FALSE)
if(DEFINED ORT_HOME AND EXISTS "${ORT_HOME}/headers/onnxruntime_cxx_api.h")
    set(HAVE_ORT TRUE)
    list(APPEND ENGINE_SOURCES OnnxSeparator.cpp)
endif()

add_library(vocalremover_audio SHARED ${ENGINE_SOURCES})

# Oboe comes from the com.google.oboe:oboe prefab package.
find_package(oboe REQUIRED CONFIG)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(vocalremover_audio
    oboe::oboe
    ${log-lib}
    ${android-lib})

if(HAVE_ORT)
    set(ORT_LIB "${ORT_HOME}/jni/${ANDROID_ABI}/libonnxruntime.so")
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES IMPORTED_LOCATION "${ORT_LIB}")
    target_include_directories(vocalremover_audio PRIVATE "${ORT_HOME}/headers")
    target_link_libraries(vocalremover_audio onnxruntime)
    target_compile_definitions(vocalremover_audio PRIVATE USE_ONNXRUNTIME=1)
    message(STATUS "ONNX Runtime enabled for ${ANDROID_ABI}: ${ORT_LIB}")
else()
    message(STATUS "ONNX Runtime not provided; building without OnnxSeparator")
endif()

target_compile_options(vocalremover_audio PRIVATE
    -Wall -Wextra -ffast-math -fno-finite-math-only)
