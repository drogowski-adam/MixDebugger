#include <mixdebugger/analysis/ResonanceAnalyzer.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("A persistent narrow ERB peak is reported as a resonance candidate")
{
    mixdebugger::analysis::TrackFeatures features;
    features.frameCount = 100;
    features.bandPower.assign(features.frameCount * features.bandCount, 1.0e-5f);
    features.active.assign(features.frameCount * features.bandCount, 1);
    features.bandUsable.fill(1);
    for (std::size_t band = 0; band < features.bandCount; ++band)
        features.centreFrequenciesHz[band] = 100.0f + static_cast<float>(band) * 100.0f;
    constexpr std::size_t resonantBand = 20;
    for (std::size_t frame = 0; frame < features.frameCount; ++frame)
        features.bandPower[frame * features.bandCount + resonantBand] = 1.0e-4f;

    const auto findings = mixdebugger::analysis::ResonanceAnalyzer{}.analyse(features);

    REQUIRE_FALSE(findings.empty());
    CHECK(findings[0].band == resonantBand);
    CHECK(findings[0].averageExcessDb > 9.0f);
    CHECK(findings[0].persistence == 1.0f);
}

TEST_CASE("High resolution resonance analysis returns concrete EQ parameters")
{
    mixdebugger::dsp::Spectrogram spectrogram;
    spectrogram.sampleRate = 48000.0;
    spectrogram.frameCount = 2000;
    spectrogram.power.assign(spectrogram.frameCount * spectrogram.binCount, 1.0e-7f);
    spectrogram.averagePower.assign(spectrogram.binCount, 1.0e-7f);
    constexpr std::size_t peakBin = 43; // około 1008 Hz przy FFT 2048 / 48 kHz
    spectrogram.averagePower[peakBin] = 1.0e-4f;
    for (std::size_t frame = 0; frame < spectrogram.frameCount; ++frame)
        spectrogram.power[frame * spectrogram.binCount + peakBin] = 1.0e-4f;

    const auto findings = mixdebugger::analysis::ResonanceAnalyzer{}.analyse(spectrogram);

    REQUIRE_FALSE(findings.empty());
    CHECK(findings[0].frequencyHz > 990.0f);
    CHECK(findings[0].frequencyHz < 1020.0f);
    CHECK(findings[0].q >= 1.0f);
    CHECK(findings[0].recommendedGainDb <= -1.5f);
    CHECK(findings[0].recommendedGainDb >= -4.0f);
    CHECK(findings[0].confidence > 0.5f);
}
