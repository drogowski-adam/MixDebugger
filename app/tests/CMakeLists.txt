add_executable(MixDebuggerAppTests
    AudioValidationTests.cpp
    CommandLineTests.cpp
    ../Source/AudioValidation.cpp
    ../Source/ReaperCommandLine.cpp
)

target_compile_features(MixDebuggerAppTests PRIVATE cxx_std_20)
target_include_directories(MixDebuggerAppTests PRIVATE ../Source)
target_link_libraries(MixDebuggerAppTests PRIVATE Catch2::Catch2WithMain juce::juce_core)

include(Catch)
catch_discover_tests(MixDebuggerAppTests)
