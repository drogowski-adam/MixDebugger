#pragma once

#include <mixdebugger/analysis/PairAnalyzer.h>
#include <mixdebugger/analysis/MixRole.h>
#include <mixdebugger/analysis/TrackRelationship.h>

#include <cstddef>
#include <vector>

namespace mixdebugger::analysis
{
enum class PairIssueSeverity
{
    low,
    medium,
    high
};

struct PairIssue
{
    std::size_t maskerIndex = 0;
    std::size_t targetIndex = 0;
    float priority = 0.0f;
    TrackRelationship relationship = TrackRelationship::independent;
};

[[nodiscard]] float pairIssuePriority(const PairSummary& summary) noexcept;
[[nodiscard]] PairIssueSeverity pairIssueSeverity(const PairSummary& summary) noexcept;

// Łączy A -> B i B -> A w jedną parę, zachowując istotniejszy kierunek.
[[nodiscard]] std::vector<PairIssue> rankPairIssues(
    const std::vector<PairSummary>& summaries,
    std::size_t trackCount,
    std::size_t maximumIssueCount = 12);

[[nodiscard]] std::vector<PairIssue> rankPairIssues(
    const std::vector<PairSummary>& summaries,
    const std::vector<MixRole>& roles,
    std::size_t maximumIssueCount = 12);

[[nodiscard]] std::vector<PairIssue> rankPairIssues(
    const std::vector<PairSummary>& summaries,
    const std::vector<TrackIdentity>& tracks,
    std::size_t maximumIssueCount = 12);
} // namespace mixdebugger::analysis
