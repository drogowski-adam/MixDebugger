#pragma once

#include <cstddef>
#include <span>

namespace mixdebugger::analysis
{
enum class PhaseAlignmentAssessment
{
    insufficientData,
    aligned,
    polaritySuspect,
    timingSuspect,
    lowCoherence
};

struct PhaseAlignmentSummary
{
    PhaseAlignmentAssessment assessment = PhaseAlignmentAssessment::insufficientData;
    float zeroLagCorrelation = 0.0f;
    float bestCorrelation = 0.0f;
    float confidence = 0.0f;
    double bestLagMilliseconds = 0.0;
};

// To jest diagnostyka zgodności czasowej i polaryzacji dla dwóch nagrań
// tego samego źródła. Nie należy stosować jej do dowolnych ścieżek miksu.
class PhaseAlignmentAnalyzer final
{
public:
    [[nodiscard]] PhaseAlignmentSummary analyse(std::span<const float> first,
                                                std::span<const float> second,
                                                double sampleRate,
                                                double maximumLagMilliseconds = 20.0) const;
};
} // namespace mixdebugger::analysis
