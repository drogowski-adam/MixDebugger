#include <mixdebugger/analysis/GroupBlendAdvisor.h>

#include <map>

namespace mixdebugger::analysis
{
namespace
{
bool isPrimaryDrumCapture(const TrackFamily family)
{
    return family == TrackFamily::kick || family == TrackFamily::snare
        || family == TrackFamily::cymbals || family == TrackFamily::toms;
}
}

std::vector<float> GroupBlendAdvisor::recommendInternalTrimsDb(
    const std::vector<GroupBlendTrackInput>& tracks) const
{
    std::map<std::string, bool> groupHasPrimaryDrums;
    for (const auto& track : tracks)
    {
        if (!track.group.empty() && isPrimaryDrumCapture(track.family))
            groupHasPrimaryDrums[track.group] = true;
    }

    std::vector<float> result(tracks.size(), 0.0f);
    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        const auto& track = tracks[index];
        if (track.group.empty() || !groupHasPrimaryDrums[track.group])
            continue;
        if (track.family == TrackFamily::drumRoom)
            result[index] = -6.0f;
        else if (track.family == TrackFamily::parallelDrums)
            result[index] = -8.0f;
    }
    return result;
}
} // namespace mixdebugger::analysis
