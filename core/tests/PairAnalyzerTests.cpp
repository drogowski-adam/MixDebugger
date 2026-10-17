#include <mixdebugger/analysis/PairAnalyzer.h>
#include <mixdebugger/analysis/TrackFeatures.h>
#include <mixdebugger/dsp/ErbFilterBank.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>

namespace
{
mixdebugger::dsp::ErbSpectrogram makeSingleBandSignal(const std::size_t frameCount,
                                                       const float activeLevelDb,
                                                       const std::size_t firstActive,
                                                       const std::size_t activeFrames)
{
    mixdebugger::dsp::ErbSpectrogram spectrum;
    spectrum.frameCount = frameCount;
    spectrum.power.assign(frameCount * spectrum.bandCount, 0.0f);
    spectrum.centreFrequenciesHz = mixdebugger::dsp::ErbFilterBank{}.centreFrequenciesHz();
    constexpr std::size_t testBand = 20;
    const auto power = std::pow(10.0f, activeLevelDb / 10.0f);

    for (std::size_t frame = firstActive;
         frame < std::min(frameCount, firstActive + activeFrames);
         ++frame)
        spectrum.power[frame * spectrum.bandCount + testBand] = power;

    spectrum.averagePower[testBand]
        = power * static_cast<float>(activeFrames) / static_cast<float>(frameCount);
    spectrum.averageDb[testBand] = 10.0f * std::log10(spectrum.averagePower[testBand] + 1.0e-12f);
    return spectrum;
}
}

TEST_CASE("Track features gate activity separately in every ERB band")
{
    auto spectrum = makeSingleBandSignal(100, -20.0f, 25, 40);
    const auto features = mixdebugger::analysis::TrackFeatureExtractor{}.extract(std::move(spectrum));

    CHECK(features.bandUsable[20] == 1);
    CHECK_FALSE(features.activeAt(24, 20));
    CHECK(features.activeAt(25, 20));
    CHECK(features.activeAt(64, 20));
    CHECK_FALSE(features.activeAt(65, 20));
    CHECK(features.bandUsable[19] == 0);
}

TEST_CASE("A band with fewer than thirty active frames is rejected")
{
    auto spectrum = makeSingleBandSignal(100, -20.0f, 10, 29);
    const auto features = mixdebugger::analysis::TrackFeatureExtractor{}.extract(std::move(spectrum));

    CHECK(features.bandUsable[20] == 0);
    CHECK_FALSE(features.activeAt(10, 20));
}

TEST_CASE("Pair masking is directional and localised in time")
{
    auto maskerSpectrum = makeSingleBandSignal(100, -10.0f, 30, 30);
    auto targetSpectrum = makeSingleBandSignal(100, -30.0f, 0, 100);
    const auto masker = mixdebugger::analysis::TrackFeatureExtractor{}.extract(
        std::move(maskerSpectrum));
    const auto target = mixdebugger::analysis::TrackFeatureExtractor{}.extract(
        std::move(targetSpectrum));

    const mixdebugger::analysis::PairAnalyzer analyzer;
    const auto forward = analyzer.analyseTimeline(masker, target);
    const auto reverse = analyzer.summarise(target, masker);

    CHECK(forward.summary.score > reverse.score);
    CHECK(forward.summary.coverage == 0.3f);
    CHECK(forward.summary.targetActiveSeconds > forward.summary.coActiveSeconds);
    CHECK(forward.summary.coActiveSeconds > 0.0f);
    CHECK(forward.summary.character == mixdebugger::analysis::ConflictCharacter::intermittent);
    CHECK(forward.at(29, 20) == 0.0f);
    CHECK(forward.at(30, 20) > 0.9f);
    CHECK(forward.at(59, 20) > 0.9f);
    CHECK(forward.at(60, 20) == 0.0f);
}

TEST_CASE("Strong bleed in a trace target band is heavily suppressed")
{
    using namespace mixdebugger::analysis;
    TrackFeatures masker;
    TrackFeatures target;
    masker.frameCount = target.frameCount = 100;
    masker.frameRate = target.frameRate = 100.0;
    masker.centreFrequenciesHz = target.centreFrequenciesHz
        = mixdebugger::dsp::ErbFilterBank{}.centreFrequenciesHz();
    masker.bandPower.assign(100 * TrackFeatures::bandCount, 0.0f);
    target.bandPower.assign(100 * TrackFeatures::bandCount, 0.0f);
    masker.active.assign(100 * TrackFeatures::bandCount, 0);
    target.active.assign(100 * TrackFeatures::bandCount, 0);
    constexpr std::size_t traceBand = 5;
    masker.bandUsable[traceBand] = target.bandUsable[traceBand] = 1;
    masker.bandSalience[traceBand] = 1.0f;
    target.bandSalience[traceBand] = 0.04f;
    for (std::size_t frame = 0; frame < 100; ++frame)
    {
        masker.bandPower[frame * TrackFeatures::bandCount + traceBand] = 0.1f;
        target.bandPower[frame * TrackFeatures::bandCount + traceBand] = 0.0001f;
        masker.active[frame * TrackFeatures::bandCount + traceBand] = 1;
        target.active[frame * TrackFeatures::bandCount + traceBand] = 1;
    }

    const auto result = PairAnalyzer{}.summarise(masker, target);
    CHECK(result.score < 0.05f);
    CHECK(result.targetSaliencePerBand[traceBand] == 0.04f);
}
