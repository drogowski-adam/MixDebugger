add_executable(MixDebuggerDspTests
    RingBufferTests.cpp
    WaveformHistoryTests.cpp
)

target_compile_features(MixDebuggerDspTests PRIVATE cxx_std_20)
target_include_directories(MixDebuggerDspTests PRIVATE ../Source)
target_link_libraries(MixDebuggerDspTests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(MixDebuggerDspTests)
