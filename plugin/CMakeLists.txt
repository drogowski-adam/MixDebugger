juce_add_plugin(MixDebuggerProbe
    COMPANY_NAME "MixDebugger"
    BUNDLE_ID "com.adidroga.mixdebugger.probe"
    PLUGIN_MANUFACTURER_CODE Adrg
    PLUGIN_CODE Mdpr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Mix Debugger Probe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MixDebuggerProbe
    PRIVATE
        Source/ProbeEditor.cpp
        Source/ProbeEditor.h
        Source/ProbeProcessor.cpp
        Source/ProbeProcessor.h
        Source/TrackRegistry.cpp
        Source/TrackRegistry.h
)

target_compile_features(MixDebuggerProbe PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(MixDebuggerProbe PRIVATE /utf-8)
endif()

target_compile_definitions(MixDebuggerProbe
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MixDebuggerProbe
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
