#include <mixdebugger/analysis/DynamicsAnalyzer.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

TEST_CASE("A constant sine is reported as very flat rather than dynamically varied")
{
    constexpr auto sampleRate = 1000.0;
    std::vector<float> samples(10000);
    for (std::size_t index = 0; index < samples.size(); ++index)
        samples[index] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 20.0f
                                        * static_cast<float>(index / sampleRate));

    const auto result = mixdebugger::analysis::DynamicsAnalyzer{}.analyse(samples, sampleRate);
    CHECK(result.assessment == mixdebugger::analysis::DynamicsAssessment::veryFlat);
    CHECK(result.shortTermRangeDb < 0.5f);
    CHECK(result.loudActiveRmsDb >= result.medianActiveRmsDb);
    CHECK(result.medianActiveRmsDb >= result.quietActiveRmsDb);
    CHECK(result.absolutePeakDb > result.activeRmsDb);
}

TEST_CASE("Clipped samples take priority over other dynamics descriptions")
{
    std::vector<float> samples(10000, 0.2f);
    samples[500] = 1.0f;
    const auto result = mixdebugger::analysis::DynamicsAnalyzer{}.analyse(samples, 1000.0);
    CHECK(result.assessment == mixdebugger::analysis::DynamicsAssessment::clippingRisk);
    CHECK(result.clippedSampleCount == 1);
}
