#include <mixdebugger/dsp/ErbFilterBank.h>
#include <mixdebugger/dsp/LanczosResampler.h>
#include <mixdebugger/dsp/StftAnalyzer.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

TEST_CASE("Lanczos resampler converts sample counts to 48 kHz")
{
    const std::vector<float> input(441, 1.0f);
    const auto output = mixdebugger::dsp::resampleLanczos(input, 44100.0, 48000.0);

    REQUIRE(output.size() == 480);
    CHECK(output[240] == Catch::Approx(1.0f).margin(1.0e-5f));
}

TEST_CASE("STFT creates 1025 bins and approximately 938 frames for ten seconds")
{
    const std::vector<float> input(480000, 0.0f);
    const auto spectrum = mixdebugger::dsp::StftAnalyzer{}.analyse(input);

    CHECK(spectrum.frameCount == 938);
    CHECK(spectrum.averagePower.size() == 1025);
    CHECK(spectrum.power.size() == spectrum.frameCount * 1025);
    CHECK_FALSE(spectrum.dominantFrequencyHz.has_value());
}

TEST_CASE("STFT locates a one kHz sine wave")
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto frequency = 1000.0;
    std::vector<float> input(48000);

    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency
                                                   * static_cast<double>(index) / sampleRate));

    const auto spectrum = mixdebugger::dsp::StftAnalyzer{}.analyse(input, sampleRate);
    const auto binWidth = sampleRate / 2048.0;
    REQUIRE(spectrum.dominantFrequencyHz.has_value());
    CHECK(*spectrum.dominantFrequencyHz == Catch::Approx(frequency).margin(binWidth));
}

TEST_CASE("ERB bank exposes forty auditory bands from 50 Hz to 16 kHz")
{
    const mixdebugger::dsp::ErbFilterBank bank;
    const auto& centres = bank.centreFrequenciesHz();

    CHECK(centres.size() == 40);
    CHECK(centres.front() == Catch::Approx(50.0f).margin(0.01f));
    CHECK(centres.back() == Catch::Approx(16000.0f).margin(0.1f));
    CHECK(std::is_sorted(centres.begin(), centres.end()));
}

TEST_CASE("ERB bank places a one kHz sine in the expected auditory band")
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto frequency = 1000.0;
    std::vector<float> input(48000);

    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency
                                                   * static_cast<double>(index) / sampleRate));

    const auto spectrum = mixdebugger::dsp::StftAnalyzer{}.analyse(input, sampleRate);
    const auto erb = mixdebugger::dsp::ErbFilterBank(sampleRate).analyse(spectrum);
    const auto strongest = std::max_element(erb.averagePower.begin(), erb.averagePower.end());
    const auto band = static_cast<std::size_t>(std::distance(erb.averagePower.begin(), strongest));

    CHECK(erb.frameCount == spectrum.frameCount);
    CHECK(erb.power.size() == erb.frameCount * 40);
    CHECK(erb.centreFrequenciesHz[band] == Catch::Approx(1000.0f).margin(200.0f));

    const auto erbPower = std::accumulate(erb.averagePower.begin(),
                                          erb.averagePower.end(),
                                          0.0f);
    auto inRangeFftPower = 0.0f;
    for (std::size_t bin = 0; bin < spectrum.averagePower.size(); ++bin)
    {
        const auto frequencyHz = static_cast<double>(bin) * sampleRate / 2048.0;
        if (frequencyHz >= 50.0 && frequencyHz <= 16000.0)
            inRangeFftPower += spectrum.averagePower[bin];
    }

    const auto energyDifferenceDb = 10.0f * std::log10(erbPower / inRangeFftPower);
    CHECK(energyDifferenceDb == Catch::Approx(0.0f).margin(0.01f));
}
