#include <mixdebugger/analysis/TrackRelationship.h>

#include <catch2/catch_test_macros.hpp>

using mixdebugger::analysis::TrackIdentity;
using mixdebugger::analysis::TrackFamily;
using mixdebugger::analysis::TrackRelationship;

TEST_CASE("Parallel drum bus can be suggested as an explicit family")
{
    CHECK(mixdebugger::analysis::classifyTrackFamily("Parallel drums")
          == TrackFamily::parallelDrums);
}

TEST_CASE("Multiple microphones of kick and snare are not independent instruments")
{
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "KICK IN BETA91", "DRUMS" },
              TrackIdentity { "KICK OUT D6", "DRUMS" })
          == TrackRelationship::sameSourceMicrophones);
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "SNARE TOP I5", "DRUMS" },
              TrackIdentity { "SNARE BOT EV", "DRUMS" })
          == TrackRelationship::sameSourceMicrophones);
}

TEST_CASE("Overheads and cymbal spot microphones are related captures")
{
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "OH L", "DRUMS" },
              TrackIdentity { "RIDE", "DRUMS" })
          == TrackRelationship::relatedMicrophones);
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "OH L", "DRUMS" },
              TrackIdentity { "OH R", "DRUMS" })
          == TrackRelationship::sameSourceMicrophones);
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "r", "DRUMS" },
              TrackIdentity { "RIDE", "DRUMS" })
          == TrackRelationship::relatedMicrophones);
}

TEST_CASE("Kick and bass remain independent masking candidates")
{
    CHECK(mixdebugger::analysis::classifyTrackRelationship(
              TrackIdentity { "KICK IN", "DRUMS" },
              TrackIdentity { "BASS DI", "BASS" })
          == TrackRelationship::independent);
}
