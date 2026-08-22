cmake_minimum_required(VERSION 3.25)

project(MixDebugger VERSION 0.1.0 LANGUAGES CXX)

add_executable(mixdebugger
    main.cpp
)

target_compile_features(mixdebugger PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(mixdebugger PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(mixdebugger PRIVATE -Wall -Wextra -Wpedantic)
endif()

