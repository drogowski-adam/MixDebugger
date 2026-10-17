cmake_minimum_required(VERSION 3.25)

project(MixDebugger VERSION 0.2.0 LANGUAGES C CXX)

if(MSVC)
    # Release binaries are self-contained; the end user does not need a separate VC++ runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(MIXDEBUGGER_BUILD_REAPER_PLUGIN "Build the experimental REAPER VST3 probe" OFF)
option(MIXDEBUGGER_BUILD_REAPER_BRIDGE "Build the native REAPER integration" ON)

include(FetchContent)
include(CTest)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

# Keep the loudness implementation independent of the UI and our spectral DSP.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
set(ENABLE_INTERNAL_QUEUE_H ON CACHE BOOL "libebur128 internal queue" FORCE)
FetchContent_Declare(ebur128
    GIT_REPOSITORY https://github.com/jiixyj/libebur128.git
    GIT_TAG v1.2.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ebur128)

if(BUILD_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.15.3
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

add_executable(mixdebugger
    main.cpp
)

target_compile_features(mixdebugger PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(mixdebugger PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(mixdebugger PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
target_link_libraries(mixdebugger PRIVATE MixDebugger::Core)
add_subdirectory(app)

if(WIN32 AND MIXDEBUGGER_BUILD_REAPER_BRIDGE)
    add_subdirectory(bridge)
    add_subdirectory(installer)
endif()

if(MIXDEBUGGER_BUILD_REAPER_PLUGIN)
    add_subdirectory(plugin)
endif()
