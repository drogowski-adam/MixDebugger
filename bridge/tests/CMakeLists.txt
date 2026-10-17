add_executable(MixDebuggerBridgeTests
    FxParameterScalingTests.cpp
    ../Source/FxParameterScaling.cpp
    ../Source/FxParameterScaling.h
    MixActionProtocolTests.cpp
    ../Source/MixActionProtocol.cpp
    ../Source/MixActionProtocol.h
    RenderTargetMappingTests.cpp
    ../Source/RenderTargetMapping.cpp
    StockFxParametersTests.cpp
    ../Source/StockFxParameters.cpp
    ../Source/StockFxParameters.h
)

target_compile_features(MixDebuggerBridgeTests PRIVATE cxx_std_20)
target_include_directories(MixDebuggerBridgeTests PRIVATE ../Source)
target_link_libraries(MixDebuggerBridgeTests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(MixDebuggerBridgeTests)
