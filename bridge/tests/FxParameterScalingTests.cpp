#include "../Source/FxParameterScaling.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using mixdebugger::reaper_bridge::findClosestNormalizedValue;

TEST_CASE("Displayed compressor units are converted to a nonlinear VST scale")
{
    // Przykładowa nieliniowa skala threshold: -60 dB przy 0, 0 dB przy 1.
    const auto normalized = findClosestNormalizedValue(-24.0,
        [](const double value) -> std::optional<double>
        {
            return -60.0 + 60.0 * value * value;
        });
    REQUIRE(normalized);
    CHECK(*normalized == Catch::Approx(std::sqrt(0.6)).margin(1.0e-6));
}

TEST_CASE("Conversion clamps safely to the nearest displayed endpoint")
{
    const auto normalized = findClosestNormalizedValue(20.0,
        [](const double value) -> std::optional<double>
        {
            return 1.0 + 9.0 * value;
        });
    REQUIRE(normalized);
    CHECK(*normalized == Catch::Approx(1.0));
}

TEST_CASE("Infinite ratio label at the endpoint does not break conversion")
{
    const auto normalized = findClosestNormalizedValue(2.5,
        [](const double value) -> std::optional<double>
        {
            if (value >= 1.0)
                return std::nullopt; // ReaComp może wyświetlić inf:1.
            return 1.0 / (1.0 - value);
        });
    REQUIRE(normalized);
    CHECK(*normalized == Catch::Approx(0.6).margin(1.0e-5));
}

TEST_CASE("Invalid formatter cannot produce an extreme parameter value")
{
    const auto normalized = findClosestNormalizedValue(2.5,
        [](double) -> std::optional<double> { return std::nullopt; });
    CHECK_FALSE(normalized);
}
