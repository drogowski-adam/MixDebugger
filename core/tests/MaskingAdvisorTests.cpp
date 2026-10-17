#include <mixdebugger/analysis/MaskingAdvisor.h>
#include <mixdebugger/dsp/ErbFilterBank.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr std::size_t problemBand = 20;

mixdebugger::analysis::TrackFeatures targetFeatures()
{
    mixdebugger::analysis::TrackFeatures result;
    result.centreFrequenciesHz = mixdebugger::dsp::ErbFilterBank{}.centreFrequenciesHz();
    result.bandSalience[problemBand] = 1.0f;
    return result;
}

mixdebugger::analysis::PairSummary strongSummary(const float coverage,
                                                  const mixdebugger::analysis::ConflictCharacter character)
{
    mixdebugger::analysis::PairSummary result;
    result.score = 0.90f;
    result.confidence = 0.90f;
    result.coverage = coverage;
    result.worstBand = static_cast<int>(problemBand);
    result.character = character;
    result.scorePerBand[problemBand] = 0.90f;
    result.coveragePerBand[problemBand] = coverage;
    result.maskerAdvantageDbPerBand[problemBand] = 8.0f;
    result.targetSaliencePerBand[problemBand] = 1.0f;
    return result;
}
}

TEST_CASE("Persistent masking produces a conservative static EQ cut")
{
    using namespace mixdebugger::analysis;
    const auto result = MaskingAdvisor{}.recommend(
        strongSummary(0.8f, ConflictCharacter::continuous), targetFeatures(),
        MixRole::support, MixRole::focus);
    CHECK(result.action == MaskingAction::staticEqCut);
    CHECK(result.gainDb < -2.0f);
    CHECK(result.gainDb >= -4.5f);
    CHECK(result.q >= 0.8f);
}

TEST_CASE("Intermittent masking produces dynamic rather than permanent EQ")
{
    using namespace mixdebugger::analysis;
    const auto result = MaskingAdvisor{}.recommend(
        strongSummary(0.3f, ConflictCharacter::intermittent), targetFeatures(),
        MixRole::background, MixRole::focus);
    CHECK(result.action == MaskingAction::dynamicEqCut);
    CHECK(result.maximumDynamicReductionDb > 2.0f);
    CHECK(result.gainDb == 0.0f);
}

TEST_CASE("A lower priority target never cuts a higher priority masker")
{
    using namespace mixdebugger::analysis;
    const auto result = MaskingAdvisor{}.recommend(
        strongSummary(0.8f, ConflictCharacter::continuous), targetFeatures(),
        MixRole::focus, MixRole::support);
    CHECK(result.action == MaskingAction::none);
}

TEST_CASE("Equal priority tracks do not produce an arbitrary correction direction")
{
    using namespace mixdebugger::analysis;
    const auto result = MaskingAdvisor{}.recommend(
        strongSummary(0.8f, ConflictCharacter::continuous), targetFeatures(),
        MixRole::focus, MixRole::focus);
    CHECK(result.action == MaskingAction::none);
}

TEST_CASE("A trace band is not turned into an EQ recommendation")
{
    using namespace mixdebugger::analysis;
    auto summary = strongSummary(0.8f, ConflictCharacter::continuous);
    summary.targetSaliencePerBand[problemBand] = 0.05f;
    const auto result = MaskingAdvisor{}.recommend(summary, targetFeatures(),
                                                    MixRole::support, MixRole::focus);
    CHECK(result.action == MaskingAction::none);
}
