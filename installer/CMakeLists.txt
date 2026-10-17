file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Payload.rc"
    CONTENT
        "101 RCDATA \"$<TARGET_FILE:MixDebuggerApp>\"\n102 RCDATA \"$<TARGET_FILE:MixDebuggerReaperBridge>\"\n"
)

add_executable(MixDebuggerSetup WIN32
    Installer.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Payload.rc"
)

add_dependencies(MixDebuggerSetup MixDebuggerApp MixDebuggerReaperBridge)
target_compile_features(MixDebuggerSetup PRIVATE cxx_std_20)
target_compile_definitions(MixDebuggerSetup PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MixDebuggerSetup PRIVATE advapi32 ole32 shell32 uuid)

if(MSVC)
    target_compile_options(MixDebuggerSetup PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

set_target_properties(MixDebuggerSetup PROPERTIES
    OUTPUT_NAME "MixDebuggerSetup"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)
