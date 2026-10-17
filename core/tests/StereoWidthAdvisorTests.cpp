#include <mixdebugger/analysis/StereoWidthAdvisor.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Strong negative stereo correlation produces a conservative width reduction")
{
    const auto result = mixdebugger::analysis::StereoWidthAdvisor{}.recommend(-0.60f, 2);
    REQUIRE(result.available);
    CHECK(result.safeToApply);
    CHECK_FALSE(result.severeMidCancellation);
    CHECK(result.widthMultiplier < 0.60f);
    CHECK(result.widthMultiplier >= 0.35f);
    CHECK(result.predictedCorrelation > -0.10f);
    CHECK(result.confidence >= 0.75f);
}

TEST_CASE("Mono and already compatible stereo do not receive a width correction")
{
    const mixdebugger::analysis::StereoWidthAdvisor advisor;
    CHECK_FALSE(advisor.recommend(-0.8f, 1).available);
    CHECK_FALSE(advisor.recommend(0.1f, 2).available);
}

TEST_CASE("Measured Mid Side energy is used instead of the equal channel assumption")
{
    const auto result = mixdebugger::analysis::StereoWidthAdvisor{}.recommend(-0.4f, 2, 9.0f);
    REQUIRE(result.available);
    CHECK(result.safeToApply);
    CHECK(result.widthMultiplier < 0.40f);
    CHECK(result.predictedCorrelation > -0.10f);
}

TEST_CASE("A nearly missing Mid channel is diagnosed but never changed automatically")
{
    const auto result = mixdebugger::analysis::StereoWidthAdvisor{}.recommend(-0.95f, 2, 100.0f);
    REQUIRE(result.available);
    CHECK_FALSE(result.safeToApply);
    CHECK(result.severeMidCancellation);
    CHECK(result.widthMultiplier == 1.0f);
    CHECK(result.predictedCorrelation == result.currentCorrelation);
}
