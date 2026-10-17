#include "ReaperCommandLine.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("REAPER manifest path is unquoted before creating a File")
{
    const auto manifest = mixdebugger::findReaperProjectManifest(
        R"(--reaper-project "C:\Users\Adam\Mix Debugger\project.json")");

    REQUIRE(manifest.has_value());
    REQUIRE(manifest->getFullPathName()
            == R"(C:\Users\Adam\Mix Debugger\project.json)");
}

TEST_CASE("REAPER manifest option also accepts equals syntax")
{
    const auto manifest = mixdebugger::findReaperProjectManifest(
        R"(--reaper-project="C:\Session Data\project.json")");

    REQUIRE(manifest.has_value());
    REQUIRE(manifest->getFileName() == "project.json");
}

TEST_CASE("A regular standalone launch has no REAPER manifest")
{
    REQUIRE_FALSE(mixdebugger::findReaperProjectManifest({}).has_value());
}
