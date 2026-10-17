#include <mixdebugger/analysis/MaskingAdvisor.h>

#include <mixdebugger/analysis/PairIssueRanking.h>

#include <algorithm>
#include <cmath>

namespace mixdebugger::analysis
{
namespace
{
int rolePriority(const MixRole role)
{
    switch (role)
    {
        case MixRole::focus: return 4;
        case MixRole::foundation: return 3;
        case MixRole::support: return 2;
        case MixRole::background: return 1;
        case MixRole::unassigned:
        case MixRole::ignore: return 0;
    }
    return 0;
}
}

MaskingRecommendation MaskingAdvisor::recommend(const PairSummary& summary,
                                                 const TrackFeatures& target,
                                                 const MixRole maskerRole,
                                                 const MixRole targetRole) const noexcept
{
    MaskingRecommendation result;
    if (summary.worstBand < 0
        || targetRole == MixRole::unassigned || targetRole == MixRole::ignore
        || maskerRole == MixRole::unassigned || maskerRole == MixRole::ignore
        // Bez hierarchii ważności nie ma obiektywnej podstawy, by zdecydować,
        // która z dwóch równorzędnych ścieżek ma zostać ściszona lub wycięta.
        // Automatyczna korekta jest więc dozwolona wyłącznie, gdy masker ma
        // jawnie niższą rolę niż chroniony cel.
        || rolePriority(maskerRole) >= rolePriority(targetRole)
        || pairIssueSeverity(summary) == PairIssueSeverity::low)
        return result;

    const auto band = static_cast<std::size_t>(summary.worstBand);
    if (band >= TrackFeatures::bandCount
        || summary.targetSaliencePerBand[band] < 0.20f
        || summary.coveragePerBand[band] < 0.08f)
        return result;

    result.frequencyHz = target.centreFrequenciesHz[band];
    result.confidence = summary.confidence;
    result.affectedTargetFraction = summary.coveragePerBand[band];

    auto broadBandCount = 0;
    for (const auto score : summary.scorePerBand)
        broadBandCount += score >= 0.55f ? 1 : 0;

    const auto advantageDb = summary.maskerAdvantageDbPerBand[band];
    if (broadBandCount >= 5)
    {
        result.action = MaskingAction::lowerMasker;
        result.gainDb = -std::clamp(0.25f * std::max(advantageDb, 4.0f), 1.0f, 3.0f);
        return result;
    }

    const auto cutDb = std::clamp(0.35f * std::max(advantageDb, 4.0f), 1.5f, 4.5f);
    result.q = std::clamp(1.0f + 0.16f * std::log2(
        std::max(80.0f, result.frequencyHz) / 500.0f), 0.8f, 1.8f);
    if (summary.coveragePerBand[band] >= 0.55f
        && summary.character == ConflictCharacter::continuous)
    {
        result.action = MaskingAction::staticEqCut;
        result.gainDb = -cutDb;
    }
    else
    {
        result.action = MaskingAction::dynamicEqCut;
        result.maximumDynamicReductionDb = cutDb;
    }
    return result;
}
} // namespace mixdebugger::analysis
