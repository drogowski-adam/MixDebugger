#pragma once

namespace mixdebugger::analysis
{
struct StereoWidthRecommendation
{
    bool available = false;
    bool safeToApply = false;
    bool severeMidCancellation = false;
    float currentCorrelation = 0.0f;
    float sideToMidPowerRatio = 0.0f;
    float widthMultiplier = 1.0f;
    float predictedCorrelation = 0.0f;
    float confidence = 0.0f;
};

// Dla silnie ujemnej korelacji szacuje względne osłabienie składowej Side,
// które przenosi wynik w okolice zera bez składania ścieżki do mono. Jeśli
// wymagałoby to niemal całkowitego usunięcia Side, zwraca diagnozę zaniku Mid,
// ale nie zezwala na automatyczną zmianę Width.
class StereoWidthAdvisor final
{
public:
    [[nodiscard]] StereoWidthRecommendation recommend(float correlation,
                                                       int channelCount,
                                                       float sideToMidPowerRatio = -1.0f) const noexcept;
};
} // namespace mixdebugger::analysis
