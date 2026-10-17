#pragma once

#include <mixdebugger/analysis/TrackFeatures.h>

#include <array>

namespace mixdebugger::analysis
{
enum class MasterBalanceAssessment
{
    unavailable,
    broadlyBalanced,
    lowHeavy,
    lowLight,
    dark,
    bright,
    midForward
};

struct MasterBalanceSummary
{
    static constexpr std::size_t regionCount = 5;
    MasterBalanceAssessment assessment = MasterBalanceAssessment::unavailable;
    std::array<float, regionCount> deviationDb{};
    std::array<float, regionCount> suggestedAdjustmentDb{};
};

class MasterBalanceAnalyzer final
{
public:
    [[nodiscard]] MasterBalanceSummary analyse(const TrackFeatures& master) const;
};
} // namespace mixdebugger::analysis
