#pragma once

#include <mixdebugger/analysis/MixRole.h>
#include <mixdebugger/analysis/PairAnalyzer.h>

namespace mixdebugger::analysis
{
enum class MaskingAction
{
    none,
    lowerMasker,
    staticEqCut,
    dynamicEqCut
};

struct MaskingRecommendation
{
    MaskingAction action = MaskingAction::none;
    float frequencyHz = 0.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    float maximumDynamicReductionDb = 0.0f;
    float confidence = 0.0f;
    float affectedTargetFraction = 0.0f;
};

// Konserwatywna translacja pomiaru na jedno konkretne działanie. Reguły nie
// ingerują w element ważniejszy od celu i nie zalecają EQ dla śladowych pasm.
class MaskingAdvisor final
{
public:
    [[nodiscard]] MaskingRecommendation recommend(const PairSummary& summary,
                                                  const TrackFeatures& target,
                                                  MixRole maskerRole,
                                                  MixRole targetRole) const noexcept;
};
} // namespace mixdebugger::analysis
