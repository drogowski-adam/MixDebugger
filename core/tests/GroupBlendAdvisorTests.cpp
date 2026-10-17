#include <mixdebugger/analysis/GroupBlendAdvisor.h>

#include <catch2/catch_test_macros.hpp>

using mixdebugger::analysis::GroupBlendAdvisor;
using mixdebugger::analysis::GroupBlendTrackInput;
using mixdebugger::analysis::TrackFamily;

TEST_CASE("Explicit room and parallel layers start behind close drum microphones")
{
    const std::vector<GroupBlendTrackInput> tracks {
        { "Drums", TrackFamily::kick },
        { "Drums", TrackFamily::snare },
        { "Drums", TrackFamily::drumRoom },
        { "Drums", TrackFamily::parallelDrums }
    };
    const auto trims = GroupBlendAdvisor{}.recommendInternalTrimsDb(tracks);
    REQUIRE(trims.size() == tracks.size());
    CHECK(trims[0] == 0.0f);
    CHECK(trims[1] == 0.0f);
    CHECK(trims[2] == -6.0f);
    CHECK(trims[3] == -8.0f);
}

TEST_CASE("A standalone room group is not attenuated by an instrument name")
{
    const std::vector<GroupBlendTrackInput> tracks {
        { "Room recordings", TrackFamily::drumRoom },
        { "", TrackFamily::drumRoom }
    };
    const auto trims = GroupBlendAdvisor{}.recommendInternalTrimsDb(tracks);
    REQUIRE(trims.size() == tracks.size());
    CHECK(trims[0] == 0.0f);
    CHECK(trims[1] == 0.0f);
}
