#pragma once

#include <string>
#include <string_view>

namespace mixdebugger::analysis
{
enum class TrackFamily
{
    unknown,
    kick,
    snare,
    cymbals,
    toms,
    drumRoom,
    bass,
    vocal,
    guitar,
    keys,
    effect,
    // Profil wybierany jawnie przez użytkownika. Nie jest automatycznie
    // przypisywany na podstawie routingu ani folderu.
    parallelDrums
};

enum class TrackRelationship
{
    independent,
    sameSourceMicrophones,
    relatedMicrophones,
    sourceAndEffect
};

struct TrackIdentity
{
    std::string name;
    std::string group;
};

[[nodiscard]] TrackFamily classifyTrackFamily(std::string_view name,
                                              std::string_view group = {});
[[nodiscard]] TrackRelationship classifyTrackRelationship(const TrackIdentity& first,
                                                           const TrackIdentity& second);
} // namespace mixdebugger::analysis
