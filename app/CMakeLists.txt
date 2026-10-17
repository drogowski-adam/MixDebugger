juce_add_gui_app(MixDebuggerApp
    PRODUCT_NAME "MixDebugger"
    COMPANY_NAME "MixDebugger"
    BUNDLE_ID "com.adidroga.mixdebugger"
)

target_sources(MixDebuggerApp
    PRIVATE
        Source/AudioValidation.cpp
        Source/AudioValidation.h
        Source/Main.cpp
        Source/ReferenceComponent.cpp
        Source/ReferenceComponent.h
        Source/ReaperCommandLine.cpp
        Source/ReaperCommandLine.h
)

target_compile_features(MixDebuggerApp PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MixDebuggerApp PRIVATE /utf-8)
endif()

target_compile_definitions(MixDebuggerApp
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        NOMINMAX
)

target_link_libraries(MixDebuggerApp
    PRIVATE
        MixDebugger::Core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
