#include <mixdebugger/analysis/PairIssueRanking.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

TEST_CASE("Pair issues combine both directions and keep the stronger one")
{
    constexpr std::size_t trackCount = 3;
    std::vector<mixdebugger::analysis::PairSummary> matrix(trackCount * trackCount);
    matrix[0 * trackCount + 1] = { .score = 0.80f, .confidence = 1.0f, .coverage = 0.8f };
    matrix[1 * trackCount + 0] = { .score = 0.30f, .confidence = 1.0f, .coverage = 0.8f };
    matrix[2 * trackCount + 0] = { .score = 0.90f, .confidence = 1.0f, .coverage = 0.9f };

    const auto issues = mixdebugger::analysis::rankPairIssues(matrix, trackCount);

    REQUIRE(issues.size() == 3);
    CHECK(issues[0].maskerIndex == 2);
    CHECK(issues[0].targetIndex == 0);
    CHECK(issues[1].maskerIndex == 0);
    CHECK(issues[1].targetIndex == 1);
}

TEST_CASE("Severity requires both a strong result and enough measured material")
{
    using namespace mixdebugger::analysis;
    PairSummary summary { .score = 0.9f, .confidence = 0.1f, .coverage = 0.9f };
    CHECK(pairIssueSeverity(summary) == PairIssueSeverity::low);

    summary.confidence = 1.0f;
    CHECK(pairIssueSeverity(summary) == PairIssueSeverity::high);
}

TEST_CASE("Multi microphone pairs are marked and strongly deprioritised")
{
    using namespace mixdebugger::analysis;
    constexpr std::size_t trackCount = 3;
    std::vector<PairSummary> matrix(trackCount * trackCount);
    matrix[0 * trackCount + 1] = { .score = 0.95f, .confidence = 1.0f, .coverage = 1.0f };
    matrix[1 * trackCount + 0] = { .score = 0.90f, .confidence = 1.0f, .coverage = 1.0f };
    matrix[0 * trackCount + 2] = { .score = 0.70f, .confidence = 1.0f, .coverage = 0.8f };
    const std::vector<TrackIdentity> tracks {
        { "SNARE TOP", "DRUMS" },
        { "SNARE BOTTOM", "DRUMS" },
        { "GUITAR", "GUITARS" }
    };

    const auto issues = rankPairIssues(matrix, tracks);

    REQUIRE(issues.size() == 3);
    CHECK(issues[0].maskerIndex == 0);
    CHECK(issues[0].targetIndex == 2);
    CHECK(issues[0].relationship == TrackRelationship::independent);
    const auto microphones = std::ranges::find_if(issues, [](const PairIssue& issue)
    {
        return issue.relationship == TrackRelationship::sameSourceMicrophones;
    });
    REQUIRE(microphones != issues.end());
    CHECK(microphones->priority < issues[0].priority);
}

TEST_CASE("Role aware ranking protects the focus from supporting tracks")
{
    using namespace mixdebugger::analysis;
    constexpr std::size_t trackCount = 2;
    std::vector<PairSummary> matrix(trackCount * trackCount);
    matrix[0 * trackCount + 1] = { .score = 0.90f, .confidence = 1.0f, .coverage = 0.8f };
    matrix[1 * trackCount + 0] = { .score = 0.75f, .confidence = 1.0f, .coverage = 0.8f };
    const std::vector roles { MixRole::focus, MixRole::support };

    const auto issues = rankPairIssues(matrix, roles);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].maskerIndex == 1);
    CHECK(issues[0].targetIndex == 0);
}
