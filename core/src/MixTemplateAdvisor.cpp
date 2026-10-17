#include <mixdebugger/analysis/MixTemplateAdvisor.h>

#include <algorithm>
#include <string>

namespace mixdebugger::analysis
{
namespace
{
struct TemplateGroup
{
    std::string key;
    std::vector<std::size_t> trackIndices;
    std::vector<TrackFamily> families;
    bool ignored = false;
};

bool containsFamily(const TemplateGroup& group, const TrackFamily family)
{
    return std::ranges::find(group.families, family) != group.families.end();
}

bool hasAnyFamily(const TemplateGroup& group,
                  const std::initializer_list<TrackFamily> families)
{
    return std::ranges::any_of(families, [&group](const TrackFamily family)
    {
        return containsFamily(group, family);
    });
}

bool isAmbienceOnly(const TemplateGroup& group)
{
    return !group.families.empty()
        && std::ranges::all_of(group.families, [](const TrackFamily family)
        {
            return family == TrackFamily::drumRoom
                || family == TrackFamily::effect
                || family == TrackFamily::parallelDrums;
        });
}
}

MixTemplateRecommendation MixTemplateAdvisor::recommend(
    const MixTemplateId templateId,
    const std::vector<MixTemplateTrackInput>& tracks) const
{
    MixTemplateRecommendation result;
    result.roles.resize(tracks.size(), MixRole::unassigned);
    if (templateId != MixTemplateId::jackieDownTheLine || tracks.empty())
        return result;

    std::vector<TemplateGroup> groups;
    groups.reserve(tracks.size());
    for (std::size_t index = 0; index < tracks.size(); ++index)
    {
        const auto key = tracks[index].group.empty()
            ? std::string("#track-") + std::to_string(index)
            : tracks[index].group;
        auto group = std::ranges::find_if(groups, [&key](const TemplateGroup& candidate)
        {
            return candidate.key == key;
        });
        if (group == groups.end())
        {
            groups.push_back({ key, {}, {}, false });
            group = std::prev(groups.end());
        }
        group->trackIndices.push_back(index);
        group->families.push_back(tracks[index].family);
        group->ignored = group->ignored || tracks[index].currentRole == MixRole::ignore;
    }

    auto focusGroup = std::ranges::find_if(groups, [](const TemplateGroup& group)
    {
        return !group.ignored && containsFamily(group, TrackFamily::vocal);
    });
    if (focusGroup == groups.end())
    {
        focusGroup = std::ranges::find_if(groups, [](const TemplateGroup& group)
        {
            return !group.ignored && containsFamily(group, TrackFamily::guitar);
        });
    }

    // Bez potwierdzonego wokalu albo gitary ten konkretny template nie ma
    // wiarygodnego punktu centralnego i nie powinien arbitralnie wybierać ścieżki.
    if (focusGroup == groups.end())
        return result;

    for (auto& group : groups)
    {
        auto role = MixRole::support;
        if (group.ignored)
            role = MixRole::ignore;
        else if (&group == &*focusGroup)
            role = MixRole::focus;
        else if (hasAnyFamily(group, { TrackFamily::bass,
                                       TrackFamily::kick,
                                       TrackFamily::snare }))
            role = MixRole::foundation;
        else if (isAmbienceOnly(group))
            role = MixRole::background;

        for (const auto trackIndex : group.trackIndices)
            result.roles[trackIndex] = role;
    }

    result.canApply = true;
    return result;
}
} // namespace mixdebugger::analysis
