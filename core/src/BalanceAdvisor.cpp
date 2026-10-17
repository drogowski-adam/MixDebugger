#include <mixdebugger/analysis/BalanceAdvisor.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>

namespace mixdebugger::analysis
{
namespace
{
constexpr auto silenceDb = -100.0f;

float powerFromDb(const float levelDb)
{
    return levelDb > silenceDb ? std::pow(10.0f, levelDb / 10.0f) : 0.0f;
}

float dbFromPower(const double power)
{
    return power > 1.0e-12 ? static_cast<float>(10.0 * std::log10(power)) : silenceDb;
}

float roleTargetDb(const MixRole role)
{
    switch (role)
    {
        case MixRole::focus: return -18.0f;
        case MixRole::foundation: return -20.5f;
        case MixRole::support: return -24.0f;
        case MixRole::background: return -27.5f;
        case MixRole::unassigned:
        case MixRole::ignore: return silenceDb;
    }
    return silenceDb;
}

float peakCeilingDb(const MixRole role)
{
    switch (role)
    {
        // Suma jest później mierzona próbka po próbce i trymowana do -6 dBFS.
        // Tutaj pilnujemy tylko, aby pojedynczy stem nie przekroczył 0 dBFS.
        case MixRole::focus: return -1.0f;
        case MixRole::foundation: return -2.0f;
        case MixRole::support: return -3.0f;
        case MixRole::background: return -4.0f;
        case MixRole::unassigned:
        case MixRole::ignore: return -12.0f;
    }
    return -12.0f;
}

float minimumMarginBelowFocusDb(const MixRole role)
{
    switch (role)
    {
        case MixRole::focus: return 0.0f;
        case MixRole::foundation: return 2.5f;
        case MixRole::support: return 6.0f;
        case MixRole::background: return 9.5f;
        case MixRole::unassigned:
        case MixRole::ignore: return 0.0f;
    }
    return 0.0f;
}

float minimumRelativeGainDb(const MixRole role)
{
    switch (role)
    {
        case MixRole::focus: return -6.0f;
        case MixRole::foundation: return -12.0f;
        case MixRole::support: return -15.0f;
        case MixRole::background: return -18.0f;
        case MixRole::unassigned:
        case MixRole::ignore: return 0.0f;
    }
    return 0.0f;
}

float maximumRelativeGainDb(const MixRole role)
{
    switch (role)
    {
        case MixRole::focus: return 18.0f;
        case MixRole::foundation: return 12.0f;
        case MixRole::support: return 9.0f;
        case MixRole::background: return 6.0f;
        case MixRole::unassigned:
        case MixRole::ignore: return 0.0f;
    }
    return 0.0f;
}

bool isUsable(const BalanceTrackInput& track)
{
    return track.role != MixRole::unassigned && track.role != MixRole::ignore
        && track.activeWindowCount >= 5 && track.activeRmsDb > -80.0f
        && track.peakDb > -100.0f;
}

std::string groupKey(const BalanceTrackInput& track, const std::size_t index)
{
    return track.group.empty() ? "#track-" + std::to_string(index) : track.group;
}

struct WorkingGroup
{
    MixRole role = MixRole::unassigned;
    std::vector<std::size_t> indices;
    double activePower = 0.0;
    float highestPeakDb = silenceDb;
    std::size_t minimumActiveWindows = 0;
    bool hasMeasuredSum = false;
};
}

MixBalanceSummary BalanceAdvisor::recommend(
    const std::vector<BalanceTrackInput>& input,
    const std::optional<float> predictedPreTrimMasterPeakDb) const
{
    MixBalanceSummary result;
    result.tracks.resize(input.size());
    if (input.empty())
        return result;

    std::map<std::pair<MixRole, std::string>, WorkingGroup> workingGroups;
    auto hasFocus = false;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const auto& track = input[index];
        if (track.role == MixRole::unassigned || track.role == MixRole::ignore)
            continue;
        auto& group = workingGroups[{ track.role, groupKey(track, index) }];
        group.role = track.role;
        group.indices.push_back(index);

        if (track.measuredGroupActiveRmsDb && track.measuredGroupPeakDb
            && track.measuredGroupActiveWindowCount >= 5)
        {
            if (!group.hasMeasuredSum)
            {
                group.activePower = powerFromDb(*track.measuredGroupActiveRmsDb);
                group.highestPeakDb = *track.measuredGroupPeakDb;
                group.minimumActiveWindows = track.measuredGroupActiveWindowCount;
                group.hasMeasuredSum = true;
            }
        }
        else if (!group.hasMeasuredSum && isUsable(track))
        {
            group.activePower += powerFromDb(track.activeRmsDb + track.internalTrimDb);
            group.highestPeakDb = std::max(
                group.highestPeakDb, track.peakDb + track.internalTrimDb);
            group.minimumActiveWindows = group.minimumActiveWindows == 0
                ? track.activeWindowCount
                : std::min(group.minimumActiveWindows, track.activeWindowCount);
        }
    }

    for (auto iterator = workingGroups.begin(); iterator != workingGroups.end();)
    {
        if (iterator->second.minimumActiveWindows < 5
            || iterator->second.activePower <= 0.0)
            iterator = workingGroups.erase(iterator);
        else
        {
            hasFocus = hasFocus || iterator->second.role == MixRole::focus;
            ++iterator;
        }
    }

    if (!hasFocus || workingGroups.empty())
        return result;

    std::array<std::size_t, 6> groupCountPerRole{};
    for (const auto& [key, group] : workingGroups)
    {
        (void) key;
        ++groupCountPerRole[static_cast<std::size_t>(group.role)];
    }

    result.groups.reserve(workingGroups.size());
    for (const auto& [key, group] : workingGroups)
    {
        const auto roleIndex = static_cast<std::size_t>(group.role);
        const auto groupCount = std::max<std::size_t>(1, groupCountPerRole[roleIndex]);
        const auto currentDb = dbFromPower(group.activePower);
        // Nie pozwalamy, aby duża liczba grup WSPARCIA automatycznie zakopała
        // każdą z nich. Pełny podział energii 10*log10(N) przy 8–12 stemach
        // ściszał np. gitary o kolejne 9–11 dB. Maksymalnie 3 dB rozdziału,
        // a bezpieczny poziom całej sumy ustala później wspólny trim headroomu.
        const auto groupSharingDb = std::min(
            3.0f, 10.0f * std::log10(static_cast<float>(groupCount)));
        const auto targetDb = roleTargetDb(group.role) - groupSharingDb;
        auto gainDb = std::clamp(targetDb - currentDb,
                                 minimumRelativeGainDb(group.role),
                                 maximumRelativeGainDb(group.role));
        const auto maximumGainFromPeak = peakCeilingDb(group.role) - group.highestPeakDb;
        const auto limitedByPeak = gainDb > maximumGainFromPeak;
        gainDb = std::min(gainDb, maximumGainFromPeak);
        gainDb = std::clamp(gainDb,
                            minimumRelativeGainDb(group.role),
                            maximumRelativeGainDb(group.role));
        BalanceGroupRecommendation recommendation;
        recommendation.group = key.second;
        recommendation.role = group.role;
        recommendation.trackIndices = group.indices;
        recommendation.currentActiveRmsDb = currentDb;
        recommendation.targetActiveRmsDb = targetDb;
        recommendation.gainDb = gainDb;
        recommendation.confidence = std::clamp(
            static_cast<float>(group.minimumActiveWindows) / 40.0f, 0.25f, 1.0f);
        recommendation.limitedByPeak = limitedByPeak;
        result.groups.push_back(std::move(recommendation));
    }

    // Focus jest kotwicą. Pozostałych ról nie ustawiamy wyłącznie względem
    // abstrakcyjnego pink-noise targetu: ich faktyczny cel wynika z poziomu,
    // który Focus mógł bezpiecznie osiągnąć po kontroli peaku. Dzięki temu
    // cichy, szczytowy lead nie zostaje przykryty poprawnie zmierzonym Tłem.
    auto focusReferenceDb = silenceDb;
    for (const auto& group : result.groups)
    {
        if (group.role == MixRole::focus)
            focusReferenceDb = std::max(
                focusReferenceDb, group.currentActiveRmsDb + group.gainDb);
    }
    if (focusReferenceDb > silenceDb)
    {
        for (auto& group : result.groups)
        {
            const auto marginDb = minimumMarginBelowFocusDb(group.role);
            if (marginDb <= 0.0f)
                continue;
            const auto roleIndex = static_cast<std::size_t>(group.role);
            const auto groupCount = std::max<std::size_t>(
                1, groupCountPerRole[roleIndex]);
            const auto groupSharingDb = std::min(
                3.0f, 10.0f * std::log10(static_cast<float>(groupCount)));
            const auto targetDb = focusReferenceDb - marginDb - groupSharingDb;
            auto gainDb = std::clamp(targetDb - group.currentActiveRmsDb,
                                     minimumRelativeGainDb(group.role),
                                     maximumRelativeGainDb(group.role));
            const auto working = workingGroups.find({ group.role, group.group });
            if (working == workingGroups.end())
                continue;
            const auto maximumGainFromPeak
                = peakCeilingDb(group.role) - working->second.highestPeakDb;
            const auto peakLimited = gainDb > maximumGainFromPeak;
            gainDb = std::clamp(std::min(gainDb, maximumGainFromPeak),
                                minimumRelativeGainDb(group.role),
                                maximumRelativeGainDb(group.role));
            group.targetActiveRmsDb = targetDb;
            group.gainDb = gainDb;
            group.limitedByPeak = group.limitedByPeak || peakLimited;
            group.hierarchyLimited
                = group.currentActiveRmsDb + group.gainDb > targetDb + 0.25f;
            result.hierarchyLimited = result.hierarchyLimited
                                   || group.hierarchyLimited;
        }
    }

    // Peak przekazany przez aplikację jest liczony po rzeczywistym zsumowaniu
    // wyrównanych stemów z powyższymi względnymi ruchami. Wspólny trim nie
    // zmienia hierarchii grup i doprowadza przewidywany peak do najwyżej -6 dBFS.
    if (predictedPreTrimMasterPeakDb && std::isfinite(*predictedPreTrimMasterPeakDb)
        && *predictedPreTrimMasterPeakDb > silenceDb)
    {
        result.headroomAvailable = true;
        result.globalHeadroomTrimDb = std::clamp(
            -6.0f - *predictedPreTrimMasterPeakDb, -24.0f, 0.0f);
        result.predictedMasterPeakDb = *predictedPreTrimMasterPeakDb
                                     + result.globalHeadroomTrimDb;
    }

    // Względne ruchy zostają na ścieżkach. Wspólny trim headroomu jest osobną
    // rekomendacją dla mastera, dzięki czemu nie spycha kilkudziesięciu faderów
    // i nie zaciemnia faktycznego balansu ról.
    for (auto& group : result.groups)
    {
        for (const auto index : group.trackIndices)
        {
            auto& recommendation = result.tracks[index];
            recommendation.gainDb = group.gainDb + input[index].internalTrimDb;
            recommendation.internalTrimDb = input[index].internalTrimDb;
            recommendation.predictedActiveRmsDb
                = input[index].activeRmsDb + recommendation.gainDb
                + result.globalHeadroomTrimDb;
            recommendation.predictedPeakDb = input[index].peakDb
                + recommendation.gainDb + result.globalHeadroomTrimDb;
            recommendation.confidence = group.confidence;
            recommendation.limitedByPeak = group.limitedByPeak;
            recommendation.hierarchyLimited = group.hierarchyLimited;
            recommendation.actionable = std::abs(recommendation.gainDb) >= 0.25f;
        }
    }

    result.available = true;
    return result;
}
} // namespace mixdebugger::analysis
