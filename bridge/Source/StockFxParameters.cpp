#include "StockFxParameters.h"

#include <algorithm>
#include <cmath>

namespace mixdebugger::reaper_bridge
{
TimeAdjustmentParameters composeTimeAdjustment(
    const double currentDelayMilliseconds,
    const double currentAdditionalSamples,
    const double correctionSeconds,
    const double sampleRate) noexcept
{
    TimeAdjustmentParameters result;
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        return result;

    const auto currentSamples = currentDelayMilliseconds * sampleRate / 1000.0
                              + currentAdditionalSamples;
    const auto maximumSamples = sampleRate * 1.0;
    result.totalSamples = std::clamp(
        std::round(currentSamples + correctionSeconds * sampleRate),
        -maximumSamples, maximumSamples);
    result.delayMilliseconds = std::clamp(
        std::round(result.totalSamples * 1000.0 / sampleRate), -1000.0, 1000.0);
    result.additionalSamples = result.totalSamples
        - std::round(result.delayMilliseconds * sampleRate / 1000.0);
    return result;
}

StereoWidthParameters composeStereoWidth(
    const double currentCenterBoostDb,
    const double relativeSideMultiplier,
    const bool updatingManagedInstance) noexcept
{
    constexpr auto stockSideCoefficient = 1.0 + 1.4142135623730951;
    auto currentMultiplier = 1.0;
    if (updatingManagedInstance && std::isfinite(currentCenterBoostDb))
    {
        const auto currentCenterGain = std::pow(10.0, currentCenterBoostDb / 20.0);
        currentMultiplier = stockSideCoefficient / (1.0 + currentCenterGain);
    }

    StereoWidthParameters result;
    result.totalSideMultiplier = std::clamp(
        currentMultiplier * std::clamp(relativeSideMultiplier, 0.10, 1.0), 0.25, 1.0);
    const auto centerGain = stockSideCoefficient / result.totalSideMultiplier - 1.0;
    result.centerBoostDb = 20.0 * std::log10(centerGain);
    result.outputGainDb = -20.0 * std::log10(1.0 + centerGain);
    return result;
}
} // namespace mixdebugger::reaper_bridge
