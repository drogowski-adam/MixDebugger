#include <mixdebugger/analysis/StereoWidthAdvisor.h>

#include <algorithm>
#include <cmath>

namespace mixdebugger::analysis
{
StereoWidthRecommendation StereoWidthAdvisor::recommend(const float correlation,
                                                          const int channelCount,
                                                          const float measuredSideToMidPower) const noexcept
{
    StereoWidthRecommendation result;
    result.currentCorrelation = std::clamp(correlation, -1.0f, 1.0f);
    if (channelCount < 2 || !std::isfinite(correlation) || correlation > -0.20f)
        return result;

    constexpr auto targetCorrelation = 0.05f;
    const auto safeCorrelation = std::clamp(correlation, -0.95f, 0.95f);
    const auto sideToMidPower = measuredSideToMidPower >= 0.0f
        && std::isfinite(measuredSideToMidPower)
        ? std::clamp(measuredSideToMidPower, 0.0001f, 10000.0f)
        : (1.0f - safeCorrelation) / (1.0f + safeCorrelation);
    result.sideToMidPowerRatio = sideToMidPower;
    const auto targetSideToMidPower = (1.0f - targetCorrelation)
                                    / (1.0f + targetCorrelation);
    const auto requiredWidthMultiplier = std::sqrt(
        targetSideToMidPower / sideToMidPower);
    // Jeżeli do odzyskania mono trzeba zostawić mniej niż 25% Side, problem
    // zwykle nie jest "za dużą szerokością", tylko zanikiem Mid / odwróceniem
    // kanału. Automatyczne Width byłoby wówczas pozorną i ryzykowną naprawą.
    if (requiredWidthMultiplier < 0.25f)
    {
        result.severeMidCancellation = true;
        result.predictedCorrelation = result.currentCorrelation;
        result.confidence = std::clamp((-correlation - 0.20f) / 0.50f, 0.0f, 1.0f);
        result.available = true;
        return result;
    }

    result.widthMultiplier = std::clamp(requiredWidthMultiplier, 0.25f, 0.90f);
    const auto newSideToMidPower = sideToMidPower
                                 * result.widthMultiplier * result.widthMultiplier;
    result.predictedCorrelation = (1.0f - newSideToMidPower)
                                / (1.0f + newSideToMidPower);
    result.confidence = std::clamp((-correlation - 0.20f) / 0.50f, 0.0f, 1.0f);
    result.safeToApply = true;
    result.available = true;
    return result;
}
} // namespace mixdebugger::analysis
