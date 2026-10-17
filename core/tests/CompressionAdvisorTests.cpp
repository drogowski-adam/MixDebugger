#include <mixdebugger/analysis/CompressionAdvisor.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Compression advisor suggests vocal leveling for uneven dynamics")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::veryUneven;
    dynamics.shortTermRangeDb = 15.0f;
    dynamics.activeRmsDb = -20.0f;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::vocal);
    CHECK(result.advice == mixdebugger::analysis::CompressionAdvice::considerLeveling);
    CHECK(result.ratioMinimum == 2.0f);
    CHECK(result.targetGainReductionMinimumDb == 3.0f);
    CHECK(result.targetGainReductionMaximumDb >= 4.8f);
    CHECK(result.targetGainReductionMaximumDb <= 5.0f);
    CHECK(result.ratio == 2.5f);
    CHECK(result.attackMilliseconds > 8.0f);
    CHECK(result.releaseMilliseconds > 60.0f);
    CHECK(result.thresholdDb < dynamics.activeRmsDb);
    const auto reductionAtActiveRms = (dynamics.activeRmsDb - result.thresholdDb)
                                    * (1.0f - 1.0f / result.ratio);
    CHECK(reductionAtActiveRms
          == Catch::Approx(result.targetGainReductionDb).margin(0.01f));
}

TEST_CASE("Compression advisor does not prescribe compression for clipped audio")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::clippingRisk;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::unknown);
    CHECK(result.advice
          == mixdebugger::analysis::CompressionAdvice::reduceInputBeforeProcessing);
    CHECK(result.targetGainReductionMaximumDb == 0.0f);
}

TEST_CASE("Compression advisor warns against compressing an already flat track")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::veryFlat;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::keys);
    CHECK(result.advice == mixdebugger::analysis::CompressionAdvice::avoidMoreCompression);
}

TEST_CASE("Explicit parallel drums profile creates a strong but bounded starting point")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::unremarkable;
    dynamics.activeRmsDb = -18.0f;
    dynamics.activeWindowCount = 100;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::parallelDrums);
    CHECK(result.advice == mixdebugger::analysis::CompressionAdvice::considerLeveling);
    CHECK(result.ratio == 8.0f);
    CHECK(result.attackMilliseconds == 10.0f);
    CHECK(result.releaseMilliseconds == 70.0f);
    CHECK(result.targetGainReductionDb == 8.0f);
}

TEST_CASE("Peak-heavy guitar receives bounded transient control")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::unremarkable;
    dynamics.activeRmsDb = -24.0f;
    dynamics.loudActiveRmsDb = -21.0f;
    dynamics.absolutePeakDb = -3.0f;
    dynamics.crestFactorDb = 21.0f;
    dynamics.shortTermRangeDb = 4.0f;
    dynamics.activeWindowCount = 100;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::guitar);
    CHECK(result.advice
          == mixdebugger::analysis::CompressionAdvice::considerPeakControl);
    CHECK(result.ratio >= 2.0f);
    CHECK(result.ratio <= 4.0f);
    CHECK(result.attackMilliseconds >= 2.0f);
    CHECK(result.attackMilliseconds <= 8.0f);
    CHECK(result.targetGainReductionDb <= 3.5f);
    CHECK(result.thresholdDb < dynamics.loudActiveRmsDb);
}

TEST_CASE("Stable track leaves the compressor at unity")
{
    mixdebugger::analysis::DynamicsSummary dynamics;
    dynamics.assessment = mixdebugger::analysis::DynamicsAssessment::unremarkable;
    dynamics.activeRmsDb = -22.0f;
    dynamics.loudActiveRmsDb = -20.0f;
    dynamics.absolutePeakDb = -8.0f;
    dynamics.crestFactorDb = 12.0f;
    dynamics.shortTermRangeDb = 3.0f;
    dynamics.activeWindowCount = 100;

    const auto result = mixdebugger::analysis::CompressionAdvisor{}.recommend(
        dynamics, mixdebugger::analysis::TrackFamily::keys);
    CHECK(result.advice == mixdebugger::analysis::CompressionAdvice::noAction);
}
