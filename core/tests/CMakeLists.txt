add_executable(MixDebuggerCoreTests
    ReferenceAnalysisTests.cpp
    BalanceAdvisorTests.cpp
    CompressionAdvisorTests.cpp
    DynamicsAnalyzerTests.cpp
    DspPipelineTests.cpp
    GroupBlendAdvisorTests.cpp
    LevelAccumulatorTests.cpp
    MaskingAdvisorTests.cpp
    MasterBalanceAnalyzerTests.cpp
    MixTemplateAdvisorTests.cpp
    PairAnalyzerTests.cpp
    PairIssueRankingTests.cpp
    PhaseAlignmentAnalyzerTests.cpp
    ResonanceAnalyzerTests.cpp
    StereoWidthAdvisorTests.cpp
    TemplateProcessingAdvisorTests.cpp
    TrackRelationshipTests.cpp
)

target_compile_features(MixDebuggerCoreTests PRIVATE cxx_std_20)
target_link_libraries(MixDebuggerCoreTests
    PRIVATE
        MixDebugger::Core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(MixDebuggerCoreTests)
