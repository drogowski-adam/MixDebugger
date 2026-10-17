#pragma once

#include <mixdebugger/analysis/TrackFeatures.h>
#include <mixdebugger/dsp/StftAnalyzer.h>

#include <cstddef>
#include <vector>

namespace mixdebugger::analysis
{
struct ResonanceFinding
{
    std::size_t band = 0;
    float frequencyHz = 0.0f;
    float averageExcessDb = 0.0f;
    float persistence = 0.0f;
    float score = 0.0f;
    float q = 1.0f;
    float recommendedGainDb = 0.0f;
    float confidence = 0.0f;
};

class ResonanceAnalyzer final
{
public:
    [[nodiscard]] std::vector<ResonanceFinding> analyse(
        const TrackFeatures& features,
        std::size_t maximumFindings = 3) const;

    // Wersja wysokiej rozdzielczości używana do podawania parametrów EQ.
    [[nodiscard]] std::vector<ResonanceFinding> analyse(
        const dsp::Spectrogram& spectrogram,
        std::size_t maximumFindings = 3) const;
};
} // namespace mixdebugger::analysis
