#pragma once

#include <mixdebugger/analysis/DynamicsAnalyzer.h>
#include <mixdebugger/analysis/TrackRelationship.h>

namespace mixdebugger::analysis
{
enum class CompressionAdvice
{
    insufficientData,
    noAction,
    reduceInputBeforeProcessing,
    avoidMoreCompression,
    considerLeveling,
    considerPeakControl
};

struct CompressionRecommendation
{
    CompressionAdvice advice = CompressionAdvice::insufficientData;
    float ratioMinimum = 1.0f;
    float ratioMaximum = 1.0f;
    float attackMinimumMilliseconds = 0.0f;
    float attackMaximumMilliseconds = 0.0f;
    float releaseMinimumMilliseconds = 0.0f;
    float releaseMaximumMilliseconds = 0.0f;
    float targetGainReductionMinimumDb = 0.0f;
    float targetGainReductionMaximumDb = 0.0f;
    // Jedno deterministyczne ustawienie startowe wybrane ze sprawdzonego zakresu.
    float ratio = 1.0f;
    float attackMilliseconds = 0.0f;
    float releaseMilliseconds = 0.0f;
    float thresholdDb = 0.0f;
    float targetGainReductionDb = 0.0f;
};

// Reguły są celowo jawne i ostrożne. Wynik opisuje cel oraz zakres startowy,
// a threshold użytkownik ustawia do podanej redukcji wzmocnienia.
class CompressionAdvisor final
{
public:
    [[nodiscard]] CompressionRecommendation recommend(const DynamicsSummary& dynamics,
                                                      TrackFamily family) const noexcept;
};
} // namespace mixdebugger::analysis
