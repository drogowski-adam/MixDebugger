#include <mixdebugger/analysis/MasterBalanceAnalyzer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace mixdebugger::analysis
{
MasterBalanceSummary MasterBalanceAnalyzer::analyse(const TrackFeatures& master) const
{
    MasterBalanceSummary result;
    if (master.frameCount == 0)
        return result;

    constexpr std::array<float, MasterBalanceSummary::regionCount + 1> edges {
        50.0f, 160.0f, 500.0f, 2000.0f, 6000.0f, 16000.0f
    };
    std::array<double, MasterBalanceSummary::regionCount> powerPerOctave{};
    for (std::size_t region = 0; region < MasterBalanceSummary::regionCount; ++region)
    {
        auto power = 0.0;
        for (std::size_t band = 0; band < TrackFeatures::bandCount; ++band)
        {
            const auto frequency = master.centreFrequenciesHz[band];
            if (frequency >= edges[region] && frequency < edges[region + 1])
                power += std::pow(10.0, static_cast<double>(master.averageDb[band]) / 10.0);
        }
        const auto octaves = std::log2(static_cast<double>(edges[region + 1] / edges[region]));
        powerPerOctave[region] = power / std::max(0.1, octaves);
    }

    std::vector<float> levels;
    levels.reserve(MasterBalanceSummary::regionCount);
    for (const auto power : powerPerOctave)
        levels.push_back(static_cast<float>(10.0 * std::log10(power + 1.0e-12)));
    auto sorted = levels;
    std::ranges::sort(sorted);
    const auto reference = sorted[sorted.size() / 2];
    for (std::size_t region = 0; region < levels.size(); ++region)
    {
        result.deviationDb[region] = levels[region] - reference;
        const auto deviation = result.deviationDb[region];
        result.suggestedAdjustmentDb[region] = std::abs(deviation) >= 2.5f
            ? std::clamp(-0.5f * deviation, -3.0f, 3.0f) : 0.0f;
    }

    const auto low = result.deviationDb[0];
    const auto lowMid = result.deviationDb[1];
    const auto mid = result.deviationDb[2];
    const auto highMid = result.deviationDb[3];
    const auto high = result.deviationDb[4];
    if (low > 5.0f && low > mid + 3.0f)
        result.assessment = MasterBalanceAssessment::lowHeavy;
    else if (low < -6.0f && lowMid < -3.0f)
        result.assessment = MasterBalanceAssessment::lowLight;
    else if (high < -6.0f && highMid < -3.0f)
        result.assessment = MasterBalanceAssessment::dark;
    else if (high > 5.0f && high > mid + 3.0f)
        result.assessment = MasterBalanceAssessment::bright;
    else if (mid > 5.0f && mid > 0.5f * (lowMid + highMid) + 3.0f)
        result.assessment = MasterBalanceAssessment::midForward;
    else
        result.assessment = MasterBalanceAssessment::broadlyBalanced;
    return result;
}
} // namespace mixdebugger::analysis
