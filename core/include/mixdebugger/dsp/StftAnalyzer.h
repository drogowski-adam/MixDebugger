#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace mixdebugger::dsp
{
struct Spectrogram
{
    static constexpr std::size_t fftSize = 2048;
    static constexpr std::size_t hopSize = 512;
    static constexpr std::size_t binCount = fftSize / 2 + 1;

    double sampleRate = 48000.0;
    std::size_t frameCount = 0;
    std::vector<float> power;
    std::vector<float> averagePower;
    std::optional<float> dominantFrequencyHz;

    [[nodiscard]] float at(std::size_t frame, std::size_t bin) const noexcept
    {
        return power[frame * binCount + bin];
    }
};

class StftAnalyzer final
{
public:
    [[nodiscard]] Spectrogram analyse(std::span<const float> monoSamples,
                                      double sampleRate = 48000.0) const;
};
} // namespace mixdebugger::dsp
