#include <mixdebugger/dsp/ErbFilterBank.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mixdebugger::dsp
{
namespace
{
constexpr double minimumFrequencyHz = 50.0;
constexpr double maximumFrequencyHz = 16000.0;

double erbNumber(const double frequencyHz)
{
    return 21.4 * std::log10(1.0 + 0.00437 * frequencyHz);
}

double frequencyFromErbNumber(const double number)
{
    return (std::pow(10.0, number / 21.4) - 1.0) / 0.00437;
}

double erbBandwidth(const double frequencyHz)
{
    return 24.7 * (0.00437 * frequencyHz + 1.0);
}
}

ErbFilterBank::ErbFilterBank(const double requestedSampleRate)
    : sampleRate(requestedSampleRate > 0.0 ? requestedSampleRate : 48000.0),
      weights(ErbSpectrogram::bandCount * Spectrogram::binCount, 0.0f)
{
    const auto firstErb = erbNumber(minimumFrequencyHz);
    const auto lastErb = erbNumber(maximumFrequencyHz);

    for (std::size_t band = 0; band < ErbSpectrogram::bandCount; ++band)
    {
        const auto position = static_cast<double>(band)
                            / static_cast<double>(ErbSpectrogram::bandCount - 1);
        centres[band] = static_cast<float>(
            frequencyFromErbNumber(firstErb + position * (lastErb - firstErb)));
    }

    for (std::size_t bin = 0; bin < Spectrogram::binCount; ++bin)
    {
        const auto frequencyHz = static_cast<double>(bin) * sampleRate
                               / static_cast<double>(Spectrogram::fftSize);
        if (frequencyHz < minimumFrequencyHz || frequencyHz > maximumFrequencyHz)
            continue;

        auto weightSum = 0.0;
        for (std::size_t band = 0; band < ErbSpectrogram::bandCount; ++band)
        {
            const auto centreHz = static_cast<double>(centres[band]);
            const auto g = (frequencyHz - centreHz) / centreHz;
            const auto p = 4.0 * centreHz / erbBandwidth(centreHz);
            const auto distance = p * std::abs(g);
            const auto weight = (1.0 + distance) * std::exp(-distance);
            weights[band * Spectrogram::binCount + bin] = static_cast<float>(weight);
            weightSum += weight;
        }

        // Normalizacja zachowuje energię binu FFT podczas rozdzielania jej
        // pomiędzy zachodzące na siebie filtry słuchowe.
        if (weightSum > 0.0)
        {
            for (std::size_t band = 0; band < ErbSpectrogram::bandCount; ++band)
                weights[band * Spectrogram::binCount + bin]
                    /= static_cast<float>(weightSum);
        }
    }
}

ErbSpectrogram ErbFilterBank::analyse(const Spectrogram& spectrogram) const
{
    ErbSpectrogram result;
    result.frameCount = spectrogram.frameCount;
    result.centreFrequenciesHz = centres;
    result.power.assign(result.frameCount * ErbSpectrogram::bandCount, 0.0f);

    for (std::size_t frame = 0; frame < result.frameCount; ++frame)
    {
        for (std::size_t band = 0; band < ErbSpectrogram::bandCount; ++band)
        {
            auto bandPower = 0.0f;
            const auto weightOffset = band * Spectrogram::binCount;
            for (std::size_t bin = 0; bin < Spectrogram::binCount; ++bin)
                bandPower += weights[weightOffset + bin] * spectrogram.at(frame, bin);

            result.power[frame * ErbSpectrogram::bandCount + band] = bandPower;
            result.averagePower[band] += bandPower;
        }
    }

    for (std::size_t band = 0; band < ErbSpectrogram::bandCount; ++band)
    {
        if (result.frameCount > 0)
            result.averagePower[band] /= static_cast<float>(result.frameCount);
        result.averageDb[band] = 10.0f * std::log10(result.averagePower[band] + 1.0e-12f);
    }

    return result;
}

const std::array<float, ErbSpectrogram::bandCount>&
ErbFilterBank::centreFrequenciesHz() const noexcept
{
    return centres;
}
} // namespace mixdebugger::dsp
