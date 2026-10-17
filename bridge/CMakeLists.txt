include(FetchContent)

FetchContent_Declare(
    reaper_sdk
    GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk.git
    GIT_TAG 490ded57668727fba21482fabc50ba9853a457bb
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(reaper_sdk)

add_library(MixDebuggerReaperBridge MODULE
    Source/BridgeExtension.cpp
)

target_compile_features(MixDebuggerReaperBridge PRIVATE cxx_std_20)
target_include_directories(MixDebuggerReaperBridge SYSTEM PRIVATE ${reaper_sdk_SOURCE_DIR}/sdk)
target_compile_definitions(MixDebuggerReaperBridge PRIVATE UNICODE _UNICODE NOMINMAX)

if(MSVC)
    target_compile_options(MixDebuggerReaperBridge PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(MixDebuggerReaperBridge PRIVATE user32 shell32 advapi32)

set_target_properties(MixDebuggerReaperBridge PROPERTIES
    PREFIX ""
    OUTPUT_NAME "reaper_mixdebugger"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
