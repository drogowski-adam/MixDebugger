#include <mixdebugger/analysis/LevelAccumulator.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using mixdebugger::analysis::LevelAccumulator;

TEST_CASE("LevelAccumulator reports silence for an empty input")
{
    const LevelAccumulator accumulator;
    const auto result = accumulator.result();

    CHECK(result.rmsDb == -100.0f);
    CHECK(result.peakDb == -100.0f);
    CHECK(result.sampleCount == 0);
}

TEST_CASE("LevelAccumulator measures a full-scale constant signal")
{
    LevelAccumulator accumulator;
    accumulator.addMonoSample(1.0f);
    accumulator.addMonoSample(1.0f);

    const auto result = accumulator.result();
    CHECK(result.rmsDb == Catch::Approx(0.0f));
    CHECK(result.peakDb == Catch::Approx(0.0f));
    CHECK(result.sampleCount == 2);
    CHECK_FALSE(result.stereoCorrelation.has_value());
}

TEST_CASE("LevelAccumulator detects identical and inverted stereo channels")
{
    constexpr float left[] = { 1.0f, -0.5f, 0.25f, -0.125f };
    constexpr float identical[] = { 1.0f, -0.5f, 0.25f, -0.125f };
    constexpr float inverted[] = { -1.0f, 0.5f, -0.25f, 0.125f };

    LevelAccumulator correlated;
    correlated.addStereoBlock(left, identical, 4);
    REQUIRE(correlated.result().stereoCorrelation.has_value());
    CHECK(*correlated.result().stereoCorrelation == Catch::Approx(1.0f));

    LevelAccumulator antiCorrelated;
    antiCorrelated.addStereoBlock(left, inverted, 4);
    REQUIRE(antiCorrelated.result().stereoCorrelation.has_value());
    CHECK(*antiCorrelated.result().stereoCorrelation == Catch::Approx(-1.0f));
    REQUIRE(antiCorrelated.result().sideToMidPowerRatio.has_value());
    CHECK(*antiCorrelated.result().sideToMidPowerRatio > 1000.0f);
}

TEST_CASE("LevelAccumulator downmixes stereo using L plus R over two")
{
    constexpr float left[] = { 1.0f, 0.5f };
    constexpr float right[] = { -1.0f, 0.5f };

    LevelAccumulator accumulator;
    accumulator.addStereoBlock(left, right, 2);

    const auto result = accumulator.result();
    CHECK(result.peakDb == Catch::Approx(-6.0206f).margin(0.001f));
    CHECK(result.rmsDb == Catch::Approx(-9.0309f).margin(0.001f));
    CHECK(result.sampleCount == 2);
    REQUIRE(result.stereoCorrelation.has_value());
    CHECK(*result.stereoCorrelation == Catch::Approx(-0.6f).margin(0.001f));
    REQUIRE(result.sideToMidPowerRatio.has_value());
    CHECK(*result.sideToMidPowerRatio == Catch::Approx(4.0f).margin(0.001f));
}
