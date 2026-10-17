#pragma once

#include <mixdebugger/dsp/StftAnalyzer.h>

#include <array>
#include <cstddef>
#include <vector>

namespace mixdebugger::dsp
{
struct ErbSpectrogram
{
    static constexpr std::size_t bandCount = 40;

    std::size_t frameCount = 0;
    std::array<float, bandCount> centreFrequenciesHz{};
    std::vector<float> power;
    std::array<float, bandCount> averagePower{};
    std::array<float, bandCount> averageDb{};

    [[nodiscard]] float at(std::size_t frame, std::size_t band) const noexcept
    {
        return power[frame * bandCount + band];
    }
};

class ErbFilterBank final
{
public:
    explicit ErbFilterBank(double sampleRate = 48000.0);

    [[nodiscard]] ErbSpectrogram analyse(const Spectrogram& spectrogram) const;
    [[nodiscard]] const std::array<float, ErbSpectrogram::bandCount>&
        centreFrequenciesHz() const noexcept;

private:
    double sampleRate = 48000.0;
    std::array<float, ErbSpectrogram::bandCount> centres{};
    std::vector<float> weights;
};
} // namespace mixdebugger::dsp
