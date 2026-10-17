#include "RenderTargetMapping.h"

#include <catch2/catch_test_macros.hpp>

using mixdebugger::reaper_bridge::trackNumberFromRenderPath;

TEST_CASE("Rendered stems map back to zero-padded REAPER track numbers")
{
    REQUIRE(trackNumberFromRenderPath(LR"(C:\renders\mixdebugger-00.wav)") == 0);
    REQUIRE(trackNumberFromRenderPath(LR"(C:\renders\mixdebugger-07.wav)") == 7);
    REQUIRE(trackNumberFromRenderPath(LR"(C:\renders\mixdebugger-056.wav)") == 56);
}

TEST_CASE("REAPER collision suffixes do not change the track number")
{
    REQUIRE(trackNumberFromRenderPath(LR"(C:\renders\mixdebugger-12-001.wav)") == 12);
}

TEST_CASE("Unrelated render targets are ignored")
{
    REQUIRE_FALSE(trackNumberFromRenderPath(LR"(C:\renders\master.wav)").has_value());
    REQUIRE_FALSE(trackNumberFromRenderPath(LR"(C:\renders\mixdebugger-track.wav)").has_value());
}
