#include <mixdebugger/dsp/StftAnalyzer.h>

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <vector>

namespace mixdebugger::dsp
{
Spectrogram StftAnalyzer::analyse(const std::span<const float> monoSamples,
                                  const double sampleRate) const
{
    Spectrogram result;
    result.sampleRate = sampleRate;
    result.averagePower.assign(Spectrogram::binCount, 0.0f);

    if (monoSamples.empty() || sampleRate <= 0.0)
        return result;

    result.frameCount = (monoSamples.size() + Spectrogram::hopSize - 1) / Spectrogram::hopSize;
    result.power.resize(result.frameCount * Spectrogram::binCount, 0.0f);

    constexpr auto fftOrder = 11;
    juce::dsp::FFT fft(fftOrder);
    std::vector<juce::dsp::Complex<float>> input(Spectrogram::fftSize);
    std::vector<juce::dsp::Complex<float>> output(Spectrogram::fftSize);
    std::vector<float> window(Spectrogram::fftSize);
    auto windowEnergy = 0.0;

    for (std::size_t index = 0; index < Spectrogram::fftSize; ++index)
    {
        const auto phase = 2.0 * std::numbers::pi * static_cast<double>(index)
                         / static_cast<double>(Spectrogram::fftSize - 1);
        window[index] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
        windowEnergy += static_cast<double>(window[index]) * static_cast<double>(window[index]);
    }

    const auto powerNormalisation = static_cast<float>(
        1.0 / (static_cast<double>(Spectrogram::fftSize) * windowEnergy));

    for (std::size_t frame = 0; frame < result.frameCount; ++frame)
    {
        const auto firstSample = frame * Spectrogram::hopSize;

        for (std::size_t index = 0; index < Spectrogram::fftSize; ++index)
        {
            const auto sampleIndex = firstSample + index;
            const auto sample = sampleIndex < monoSamples.size() ? monoSamples[sampleIndex] : 0.0f;
            input[index] = { sample * window[index], 0.0f };
        }

        fft.perform(input.data(), output.data(), false);

        for (std::size_t bin = 0; bin < Spectrogram::binCount; ++bin)
        {
            const auto oneSidedFactor = (bin == 0 || bin + 1 == Spectrogram::binCount) ? 1.0f : 2.0f;
            const auto binPower = std::norm(output[bin]) * powerNormalisation * oneSidedFactor;
            result.power[frame * Spectrogram::binCount + bin] = binPower;
            result.averagePower[bin] += binPower;
        }
    }

    for (auto& binPower : result.averagePower)
        binPower /= static_cast<float>(result.frameCount);

    const auto strongestBin = std::max_element(result.averagePower.begin() + 1,
                                               result.averagePower.end());
    if (strongestBin != result.averagePower.end() && *strongestBin > 0.0f)
    {
        const auto binIndex = static_cast<std::size_t>(
            std::distance(result.averagePower.begin(), strongestBin));
        result.dominantFrequencyHz = static_cast<float>(
            static_cast<double>(binIndex) * sampleRate / static_cast<double>(Spectrogram::fftSize));
    }
    return result;
}
} // namespace mixdebugger::dsp
