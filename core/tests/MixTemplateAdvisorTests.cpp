#include <mixdebugger/analysis/MixTemplateAdvisor.h>

#include <catch2/catch_test_macros.hpp>

using mixdebugger::analysis::MixRole;
using mixdebugger::analysis::MixTemplateAdvisor;
using mixdebugger::analysis::MixTemplateId;
using mixdebugger::analysis::MixTemplateTrackInput;
using mixdebugger::analysis::TrackFamily;

TEST_CASE("Jackie template builds a vocal-led post-punk role hierarchy")
{
    const std::vector<MixTemplateTrackInput> tracks {
        { "Lead", TrackFamily::vocal },
        { "Rhythm", TrackFamily::bass },
        { "Drums", TrackFamily::kick },
        { "Drums", TrackFamily::snare },
        { "Guitar A", TrackFamily::guitar },
        { "Guitar B", TrackFamily::guitar },
        { "Ambience", TrackFamily::drumRoom },
        { "Ambience", TrackFamily::effect }
    };

    const auto result = MixTemplateAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine, tracks);
    REQUIRE(result.canApply);
    REQUIRE(result.roles.size() == tracks.size());
    CHECK(result.roles[0] == MixRole::focus);
    CHECK(result.roles[1] == MixRole::foundation);
    CHECK(result.roles[2] == MixRole::foundation);
    CHECK(result.roles[3] == MixRole::foundation);
    CHECK(result.roles[4] == MixRole::support);
    CHECK(result.roles[5] == MixRole::support);
    CHECK(result.roles[6] == MixRole::background);
    CHECK(result.roles[7] == MixRole::background);
}

TEST_CASE("Jackie template uses the first confirmed guitar group when there is no vocal")
{
    const std::vector<MixTemplateTrackInput> tracks {
        { "Guitars A", TrackFamily::guitar },
        { "Guitars B", TrackFamily::guitar },
        { "Rhythm", TrackFamily::bass }
    };

    const auto result = MixTemplateAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine, tracks);
    REQUIRE(result.canApply);
    CHECK(result.roles[0] == MixRole::focus);
    CHECK(result.roles[1] == MixRole::support);
    CHECK(result.roles[2] == MixRole::foundation);
}

TEST_CASE("Template keeps groups coherent and preserves ignored groups")
{
    const std::vector<MixTemplateTrackInput> tracks {
        { "Lead", TrackFamily::vocal },
        { "Drums", TrackFamily::kick },
        { "Drums", TrackFamily::drumRoom },
        { "Muted", TrackFamily::effect, MixRole::ignore }
    };

    const auto result = MixTemplateAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine, tracks);
    REQUIRE(result.canApply);
    CHECK(result.roles[1] == MixRole::foundation);
    CHECK(result.roles[2] == MixRole::foundation);
    CHECK(result.roles[3] == MixRole::ignore);
}

TEST_CASE("Template refuses to guess a focus without a confirmed vocal or guitar")
{
    const std::vector<MixTemplateTrackInput> tracks {
        { "Unknown", TrackFamily::unknown },
        { "Rhythm", TrackFamily::bass }
    };

    const auto result = MixTemplateAdvisor{}.recommend(
        MixTemplateId::jackieDownTheLine, tracks);
    CHECK_FALSE(result.canApply);
}
