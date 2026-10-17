#include <mixdebugger/analysis/PairIssueRanking.h>

#include <algorithm>

namespace mixdebugger::analysis
{
namespace
{
float relationshipWeight(const TrackRelationship relationship)
{
    switch (relationship)
    {
        case TrackRelationship::sameSourceMicrophones: return 0.05f;
        case TrackRelationship::relatedMicrophones: return 0.15f;
        case TrackRelationship::sourceAndEffect: return 0.70f;
        case TrackRelationship::independent: return 1.0f;
    }
    return 1.0f;
}

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

float roleDirectionWeight(const MixRole masker, const MixRole target)
{
    if (masker == MixRole::ignore || target == MixRole::ignore
        || masker == MixRole::unassigned || target == MixRole::unassigned)
        return 0.0f;
    const auto difference = rolePriority(target) - rolePriority(masker);
    if (difference >= 2)
        return 1.35f;
    if (difference == 1)
        return 1.18f;
    if (difference == 0)
        return 0.85f;
    return 0.30f;
}

std::vector<PairIssue> rankPairIssuesImpl(const std::vector<PairSummary>& summaries,
                                          const std::size_t trackCount,
                                          const std::vector<TrackIdentity>* identities,
                                          const std::vector<MixRole>* roles,
                                          const std::size_t maximumIssueCount)
{
    std::vector<PairIssue> issues;
    if (trackCount < 2 || summaries.size() < trackCount * trackCount
        || maximumIssueCount == 0)
        return issues;

    issues.reserve(trackCount * (trackCount - 1) / 2);
    for (std::size_t first = 0; first < trackCount; ++first)
    {
        for (std::size_t second = first + 1; second < trackCount; ++second)
        {
            const auto relationship = identities != nullptr
                ? classifyTrackRelationship((*identities)[first], (*identities)[second])
                : TrackRelationship::independent;
            const auto forwardIndex = first * trackCount + second;
            const auto reverseIndex = second * trackCount + first;
            const auto forwardRoleWeight = roles != nullptr
                ? roleDirectionWeight((*roles)[first], (*roles)[second]) : 1.0f;
            const auto reverseRoleWeight = roles != nullptr
                ? roleDirectionWeight((*roles)[second], (*roles)[first]) : 1.0f;
            const auto forwardPriority = pairIssuePriority(summaries[forwardIndex])
                                       * forwardRoleWeight;
            const auto reversePriority = pairIssuePriority(summaries[reverseIndex])
                                       * reverseRoleWeight;
            const auto weight = relationshipWeight(relationship);

            if (reversePriority > forwardPriority)
                issues.push_back({ second, first, reversePriority * weight, relationship });
            else
                issues.push_back({ first, second, forwardPriority * weight, relationship });
        }
    }

    std::stable_sort(issues.begin(), issues.end(), [](const PairIssue& left,
                                                       const PairIssue& right)
    {
        return left.priority > right.priority;
    });
    if (issues.size() > maximumIssueCount)
        issues.resize(maximumIssueCount);
    return issues;
}
}

float pairIssuePriority(const PairSummary& summary) noexcept
{
    const auto dataWeight = 0.7f
                          + 0.2f * std::clamp(summary.confidence, 0.0f, 1.0f)
                          + 0.1f * std::clamp(summary.coverage, 0.0f, 1.0f);
    return std::clamp(summary.score, 0.0f, 1.0f) * dataWeight;
}

PairIssueSeverity pairIssueSeverity(const PairSummary& summary) noexcept
{
    const auto priority = pairIssuePriority(summary);
    if (summary.confidence >= 0.70f && summary.coverage >= 0.15f && priority >= 0.78f)
        return PairIssueSeverity::high;
    if (summary.confidence >= 0.35f && summary.coverage >= 0.08f && priority >= 0.55f)
        return PairIssueSeverity::medium;
    return PairIssueSeverity::low;
}

std::vector<PairIssue> rankPairIssues(const std::vector<PairSummary>& summaries,
                                      const std::size_t trackCount,
                                      const std::size_t maximumIssueCount)
{
    return rankPairIssuesImpl(summaries, trackCount, nullptr, nullptr, maximumIssueCount);
}

std::vector<PairIssue> rankPairIssues(const std::vector<PairSummary>& summaries,
                                      const std::vector<MixRole>& roles,
                                      const std::size_t maximumIssueCount)
{
    return rankPairIssuesImpl(summaries, roles.size(), nullptr, &roles, maximumIssueCount);
}

std::vector<PairIssue> rankPairIssues(const std::vector<PairSummary>& summaries,
                                      const std::vector<TrackIdentity>& tracks,
                                      const std::size_t maximumIssueCount)
{
    return rankPairIssuesImpl(summaries, tracks.size(), &tracks, nullptr, maximumIssueCount);
}
} // namespace mixdebugger::analysis
