#include <mixdebugger/analysis/BalanceAdvisor.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using mixdebugger::analysis::BalanceAdvisor;
using mixdebugger::analysis::BalanceTrackInput;
using mixdebugger::analysis::MixRole;

TEST_CASE("Balance advisor requires an explicit focus")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Drums", MixRole::foundation, -18.0f, -3.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks, -2.0f);
    CHECK_FALSE(result.available);
    REQUIRE(result.tracks.size() == 1);
    CHECK_FALSE(result.tracks[0].actionable);
}

TEST_CASE("Background is placed below focus in a neutral starting balance")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -24.0f, -10.0f, 100 },
        { "FX", MixRole::background, -24.0f, -10.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    REQUIRE(result.tracks.size() == 2);
    CHECK(result.tracks[0].gainDb > 0.0f);
    CHECK(result.tracks[1].gainDb < 0.0f);
    CHECK(result.tracks[0].predictedActiveRmsDb
          > result.tracks[1].predictedActiveRmsDb + 6.9f);
}

TEST_CASE("Peak limited focus does not destroy the background fader")
{
    const std::vector<BalanceTrackInput> tracks {
        // Focus ma duży crest factor, więc ceiling pozwala mu tylko na -1 dB ruchu.
        { "Lead", MixRole::focus, -30.0f, -3.0f, 100 },
        { "Bed", MixRole::background, -20.0f, -10.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    REQUIRE(result.tracks.size() == 2);
    CHECK(result.tracks[0].limitedByPeak);
    CHECK(result.tracks[0].gainDb >= -3.0f);
    CHECK(result.tracks[1].gainDb >= -18.0f);
    CHECK(result.tracks[0].predictedActiveRmsDb
          > result.tracks[1].predictedActiveRmsDb);
}

TEST_CASE("Impossible role hierarchy is reported instead of being hidden")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -60.0f, -1.0f, 100 },
        { "Bed", MixRole::background, -10.0f, -2.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK(result.hierarchyLimited);
    CHECK(result.tracks[1].hierarchyLimited);
    CHECK(result.tracks[1].gainDb == -18.0f);
}

TEST_CASE("Tracks in one group keep their internal level difference")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Vocal", MixRole::focus, -20.0f, -8.0f, 100 },
        { "Drums", MixRole::foundation, -15.0f, -2.0f, 100 },
        { "Drums", MixRole::foundation, -21.0f, -8.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK(result.tracks[1].gainDb == result.tracks[2].gainDb);
    CHECK(result.tracks[1].predictedActiveRmsDb
          - result.tracks[2].predictedActiveRmsDb == 6.0f);
}

TEST_CASE("Explicit internal layer trim is added to the common group movement")
{
    std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 },
        { "Drums", MixRole::foundation, -20.0f, -8.0f, 100 },
        { "Drums", MixRole::foundation, -20.0f, -8.0f, 100 }
    };
    tracks[2].internalTrimDb = -6.0f;
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK(result.tracks[1].gainDb == result.tracks[2].gainDb + 6.0f);
    CHECK(result.tracks[1].predictedActiveRmsDb
          == result.tracks[2].predictedActiveRmsDb + 6.0f);
}

TEST_CASE("Two groups in one role share its energy budget")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 },
        { "Guitars", MixRole::support, -28.0f, -12.0f, 100 },
        { "Keys", MixRole::support, -28.0f, -12.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    REQUIRE(result.groups.size() == 3);
    CHECK(result.tracks[1].predictedActiveRmsDb == -27.0f);
    CHECK(result.tracks[2].predictedActiveRmsDb == -27.0f);
}

TEST_CASE("Many support groups are not progressively buried")
{
    std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 }
    };
    for (auto index = 0; index < 8; ++index)
        tracks.push_back({ "Support-" + std::to_string(index),
                           MixRole::support, -28.0f, -12.0f, 100 });

    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    // Wsparcie pozostaje 6 dB pod Focusem, a maksymalny podział grup to 3 dB.
    for (std::size_t index = 1; index < result.tracks.size(); ++index)
        CHECK(result.tracks[index].predictedActiveRmsDb == -27.0f);
}

TEST_CASE("Measured aligned group sum replaces the uncorrelated power estimate")
{
    std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 },
        { "MultiMic", MixRole::foundation, -20.0f, -8.0f, 100 },
        { "MultiMic", MixRole::foundation, -20.0f, -8.0f, 100 }
    };
    for (auto index : { 1U, 2U })
    {
        tracks[index].measuredGroupActiveRmsDb = -14.0f;
        tracks[index].measuredGroupPeakDb = -4.0f;
        tracks[index].measuredGroupActiveWindowCount = 100;
    }
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    REQUIRE(result.groups.size() == 2);
    const auto group = std::ranges::find_if(result.groups, [](const auto& item)
    {
        return item.group == "MultiMic";
    });
    REQUIRE(group != result.groups.end());
    CHECK(group->currentActiveRmsDb == -14.0f);
    CHECK(result.tracks[1].gainDb == result.tracks[2].gainDb);
}

TEST_CASE("Master headroom trim remains separate from relative track moves")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 },
        { "Bass", MixRole::foundation, -20.5f, -8.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks, -1.0f);
    REQUIRE(result.available);
    CHECK(result.headroomAvailable);
    CHECK(result.globalHeadroomTrimDb == -5.0f);
    CHECK(result.tracks[0].gainDb == 0.0f);
    CHECK(result.tracks[1].gainDb == 0.0f);
    CHECK(result.tracks[0].predictedActiveRmsDb == -23.0f);
    CHECK(result.tracks[1].predictedActiveRmsDb == -25.5f);
    CHECK(result.predictedMasterPeakDb == -6.0f);
}

TEST_CASE("Focus and background moves stay inside conservative fader limits")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -8.0f, -1.0f, 100 },
        { "Backing", MixRole::background, -8.0f, -1.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK(result.tracks[0].gainDb >= -6.0f);
    CHECK(result.tracks[1].gainDb >= -18.0f);
}

TEST_CASE("Headroom is not claimed without an analysed master render")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK_FALSE(result.headroomAvailable);
}

TEST_CASE("Exact predicted post-balance peak can require a large common trim")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -40.0f, -18.0f, 100 },
        { "Bed", MixRole::support, -25.0f, -2.0f, 100 }
    };
    // 8 dBFS jest wynikiem dokładnego przeliczenia sumy po względnych ruchach.
    const auto result = BalanceAdvisor{}.recommend(tracks, 8.0f);
    REQUIRE(result.available);
    CHECK(result.globalHeadroomTrimDb == -14.0f);
    CHECK(result.predictedMasterPeakDb <= -6.0f);
}

TEST_CASE("Ignored tracks never receive an action")
{
    const std::vector<BalanceTrackInput> tracks {
        { "Lead", MixRole::focus, -18.0f, -8.0f, 100 },
        { "Reference", MixRole::ignore, -5.0f, -0.1f, 100 }
    };
    const auto result = BalanceAdvisor{}.recommend(tracks);
    REQUIRE(result.available);
    CHECK_FALSE(result.tracks[1].actionable);
    CHECK(result.tracks[1].gainDb == 0.0f);
}
