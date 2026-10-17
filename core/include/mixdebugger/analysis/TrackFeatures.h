#pragma once

#include <mixdebugger/dsp/ErbFilterBank.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mixdebugger::analysis
{
struct TrackFeatures
{
    static constexpr std::size_t bandCount = dsp::ErbSpectrogram::bandCount;

    std::size_t frameCount = 0;
    double frameRate = 48000.0 / static_cast<double>(dsp::Spectrogram::hopSize);
    std::array<float, bandCount> centreFrequenciesHz{};
    std::array<float, bandCount> averageDb{};
    std::array<float, bandCount> referenceDb{};
    // 0 oznacza pasmo śladowe względem charakteru ścieżki, 1 pasmo należące
    // do jej głównej zawartości. Chroni ranking przed zgłaszaniem cichego bleed'u.
    std::array<float, bandCount> bandSalience{};
    std::array<std::uint8_t, bandCount> bandUsable{};
    std::vector<float> bandPower;
    std::vector<std::uint8_t> active;

    [[nodiscard]] float powerAt(std::size_t frame, std::size_t band) const noexcept;
    [[nodiscard]] bool activeAt(std::size_t frame, std::size_t band) const noexcept;
};

class TrackFeatureExtractor final
{
public:
    static constexpr std::size_t minimumActiveFrames = 30;

    [[nodiscard]] TrackFeatures extract(dsp::ErbSpectrogram spectrogram) const;
};
} // namespace mixdebugger::analysis
