#include "../Source/StockFxParameters.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("Time Adjustment Delay preserves a sub millisecond correction as samples")
{
    using mixdebugger::reaper_bridge::composeTimeAdjustment;
    const auto result = composeTimeAdjustment(0.0, 0.0, -0.000125, 48000.0);
    CHECK(result.delayMilliseconds == 0.0);
    CHECK(result.additionalSamples == -6.0);
    CHECK(result.totalSamples == -6.0);
}

TEST_CASE("Time Adjustment Delay updates the existing managed correction")
{
    using mixdebugger::reaper_bridge::composeTimeAdjustment;
    const auto result = composeTimeAdjustment(5.0, 6.0, 0.001, 48000.0);
    CHECK(result.totalSamples == 294.0);
    CHECK(result.delayMilliseconds == 6.0);
    CHECK(result.additionalSamples == 6.0);
}

TEST_CASE("Stock Stereo Width keeps Mid at unity and applies the requested Side factor")
{
    using mixdebugger::reaper_bridge::composeStereoWidth;
    const auto result = composeStereoWidth(0.0, 0.5, false);
    const auto centerGain = std::pow(10.0, result.centerBoostDb / 20.0);
    const auto outputGain = std::pow(10.0, result.outputGainDb / 20.0);
    const auto midCoefficient = (1.0 + centerGain) * outputGain;
    const auto sideCoefficient = (1.0 + std::sqrt(2.0)) * outputGain;
    CHECK(midCoefficient == Catch::Approx(1.0).margin(0.000001));
    CHECK(sideCoefficient == Catch::Approx(0.5).margin(0.000001));
    CHECK(result.totalSideMultiplier == Catch::Approx(0.5));
}

TEST_CASE("Stock Stereo Width composes iterative relative changes")
{
    using mixdebugger::reaper_bridge::composeStereoWidth;
    const auto first = composeStereoWidth(0.0, 0.6, false);
    const auto second = composeStereoWidth(first.centerBoostDb, 0.5, true);
    CHECK(second.totalSideMultiplier == Catch::Approx(0.3).margin(0.000001));
}
