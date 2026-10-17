#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace mixdebugger::analysis
{
enum class DynamicsAssessment
{
    insufficientData,
    unremarkable,
    veryFlat,
    veryUneven,
    clippingRisk
};

struct DynamicsSummary
{
    DynamicsAssessment assessment = DynamicsAssessment::insufficientData;
    float shortTermRangeDb = 0.0f;
    float crestFactorDb = 0.0f;
    float activeRmsDb = -100.0f;
    float quietActiveRmsDb = -100.0f;
    float medianActiveRmsDb = -100.0f;
    float loudActiveRmsDb = -100.0f;
    float absolutePeakDb = -100.0f;
    std::uint64_t clippedSampleCount = 0;
    std::size_t activeWindowCount = 0;
};

class DynamicsAnalyzer final
{
public:
    [[nodiscard]] DynamicsSummary analyse(std::span<const float> monoSamples,
                                          double sampleRate) const;
};
} // namespace mixdebugger::analysis
