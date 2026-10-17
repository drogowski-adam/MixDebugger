add_library(MixDebuggerCore STATIC
    src/ReferenceAnalysis.cpp
    src/BalanceAdvisor.cpp
    src/CompressionAdvisor.cpp
    src/DynamicsAnalyzer.cpp
    src/ErbFilterBank.cpp
    src/GroupBlendAdvisor.cpp
    src/LanczosResampler.cpp
    src/MaskingAdvisor.cpp
    src/MasterBalanceAnalyzer.cpp
    src/MixTemplateAdvisor.cpp
    src/PairAnalyzer.cpp
    src/PairIssueRanking.cpp
    src/PhaseAlignmentAnalyzer.cpp
    src/ResonanceAnalyzer.cpp
    src/StftAnalyzer.cpp
    src/StereoWidthAdvisor.cpp
    src/TemplateProcessingAdvisor.cpp
    src/TrackRelationship.cpp
    src/TrackFeatures.cpp
)
add_library(MixDebugger::Core ALIAS MixDebuggerCore)

target_compile_features(MixDebuggerCore PUBLIC cxx_std_20)
target_include_directories(MixDebuggerCore PUBLIC include)
target_link_libraries(MixDebuggerCore PUBLIC juce::juce_dsp juce::juce_audio_formats ebur128)
target_include_directories(MixDebuggerCore PRIVATE ${ebur128_SOURCE_DIR}/ebur128)

if(MSVC)
    target_compile_options(MixDebuggerCore PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(MixDebuggerCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
