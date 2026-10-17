#include <mixdebugger/analysis/TemplateProcessingAdvisor.h>

#include <catch2/catch_test_macros.hpp>

using namespace mixdebugger::analysis;

namespace
{
DynamicsSummary usableDynamics()
{
    DynamicsSummary result;
    result.assessment = DynamicsAssessment::unremarkable;
    result.activeRmsDb = -20.0f;
    result.loudActiveRmsDb = -18.0f;
    result.crestFactorDb = 10.0f;
    result.shortTermRangeDb = 3.0f;
    result.activeWindowCount = 100;
    return result;
}
}

TEST_CASE("Jackie processing gives lead vocal a stock tonal chain")
{
    const auto result = TemplateProcessingAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine,
        { TrackFamily::vocal, MixRole::focus, usableDynamics() });
    REQUIRE(result.available);
    CHECK(result.stockEqPreset == "stock - Wide male vocal");
    CHECK(result.stockCompressorPreset == "stock - Modern Vocal");
    CHECK_FALSE(result.compressorActive);
    CHECK(result.saturationAmountPercent == 7.0f);
}

TEST_CASE("Background vocal uses a quieter role-specific stock chain")
{
    const auto result = TemplateProcessingAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine,
        { TrackFamily::vocal, MixRole::background, usableDynamics() });
    REQUIRE(result.available);
    CHECK(result.stockEqPreset == "stock - Basic 11 band BackGroundVocals");
    CHECK(result.stockCompressorPreset == "stock - BackGroundVocals");
    CHECK(result.saturationAmountPercent == 3.5f);
}

TEST_CASE("Clipping and flat dynamics keep compressor neutral and block saturation")
{
    auto clipping = usableDynamics();
    clipping.assessment = DynamicsAssessment::clippingRisk;
    clipping.clippedSampleCount = 1;
    const auto clipped = TemplateProcessingAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine,
        { TrackFamily::guitar, MixRole::support, clipping });
    CHECK(clipped.stockEqPreset == "stock - Basic 11 band Fender or Mesa");
    CHECK(clipped.stockCompressorPreset == "stock - distorted electrtic guitar");
    CHECK_FALSE(clipped.compressorActive);
    CHECK(clipped.saturationAmountPercent == 0.0f);

    auto flat = usableDynamics();
    flat.assessment = DynamicsAssessment::veryFlat;
    const auto compressed = TemplateProcessingAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine,
        { TrackFamily::bass, MixRole::foundation, flat });
    CHECK(compressed.stockCompressorPreset == "stock - Driving Rock Bass");
    CHECK_FALSE(compressed.compressorActive);
    CHECK(compressed.saturationAmountPercent == 0.0f);
}

TEST_CASE("Every confirmed family receives an EQ and compressor slot")
{
    for (const auto family : { TrackFamily::toms, TrackFamily::drumRoom,
                               TrackFamily::effect, TrackFamily::unknown })
    {
        const auto result = TemplateProcessingAdvisor{}.recommend(
            MixTemplateId::jackieDownTheLine,
            { family, MixRole::support, usableDynamics() });
        REQUIRE(result.available);
        CHECK_FALSE(result.stockEqPreset.empty());
        CHECK_FALSE(result.stockCompressorPreset.empty());
    }
}

TEST_CASE("Template compressor adopts peak-control measurements")
{
    auto dynamics = usableDynamics();
    dynamics.crestFactorDb = 22.0f;
    dynamics.absolutePeakDb = -2.0f;
    const auto result = TemplateProcessingAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine,
        { TrackFamily::guitar, MixRole::support, dynamics });
    REQUIRE(result.available);
    CHECK(result.compressorActive);
    CHECK(result.compression.advice == CompressionAdvice::considerPeakControl);
    CHECK(result.compression.targetGainReductionDb <= 3.5f);
}
