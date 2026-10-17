#include <mixdebugger/analysis/TrackRelationship.h>

#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace mixdebugger::analysis
{
namespace
{
std::string normalise(const std::string_view text)
{
    std::string result;
    result.reserve(text.size() + 2);
    result.push_back(' ');
    auto previousWasSpace = true;
    for (const auto character : text)
    {
        const auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value) != 0)
        {
            result.push_back(static_cast<char>(std::tolower(value)));
            previousWasSpace = false;
        }
        else if (!previousWasSpace)
        {
            result.push_back(' ');
            previousWasSpace = true;
        }
    }
    if (!previousWasSpace)
        result.push_back(' ');
    return result;
}

bool containsAny(const std::string& normalised,
                 const std::initializer_list<std::string_view> words)
{
    return std::ranges::any_of(words, [&normalised](const std::string_view word)
    {
        return normalised.find(" " + std::string(word) + " ") != std::string::npos;
    });
}

TrackFamily classifyNormalised(const std::string& value)
{
    if (containsAny(value, { "parallel drums", "parallel drum", "drum parallel",
                             "parallel comp", "parallel compression" }))
        return TrackFamily::parallelDrums;
    if (containsAny(value, { "reverb", "rev", "delay", "dly", "echo", "fx" }))
        return TrackFamily::effect;
    if (containsAny(value, { "overhead", "overheads", "oh", "ride", "hihat", "hh",
                             "hat", "cymbal", "cymbals", "crash" }))
        return TrackFamily::cymbals;
    if (containsAny(value, { "snare", "werbel" }))
        return TrackFamily::snare;
    if (containsAny(value, { "kick", "bd", "bassdrum", "stopa" }))
        return TrackFamily::kick;
    if (containsAny(value, { "tom", "toms", "floor" }))
        return TrackFamily::toms;
    if (containsAny(value, { "room", "rooms", "amb", "ambient" }))
        return TrackFamily::drumRoom;
    if (containsAny(value, { "vocal", "vocals", "vox", "voc", "wokal" }))
        return TrackFamily::vocal;
    if (containsAny(value, { "bass", "bas" }))
        return TrackFamily::bass;
    if (containsAny(value, { "guitar", "gtr", "git", "gitara" }))
        return TrackFamily::guitar;
    if (containsAny(value, { "keys", "key", "synth", "piano", "organ" }))
        return TrackFamily::keys;
    return TrackFamily::unknown;
}

bool hasCaptureHint(const std::string& value)
{
    return containsAny(value, { "top", "bottom", "bot", "in", "out", "inside", "outside",
                                "mic", "microphone", "close", "sub", "up", "down" });
}

bool isDrumCaptureFamily(const TrackFamily family)
{
    return family == TrackFamily::kick || family == TrackFamily::snare
        || family == TrackFamily::cymbals || family == TrackFamily::toms
        || family == TrackFamily::drumRoom;
}
}

TrackFamily classifyTrackFamily(const std::string_view name, const std::string_view group)
{
    const auto normalisedName = normalise(name);
    const auto normalisedGroup = normalise(group);
    const auto fromName = classifyNormalised(normalisedName);
    if (fromName != TrackFamily::unknown)
        return fromName;

    // Krótkie nazwy L/R są częste dla par overheadów. Stosujemy tę regułę tylko
    // wewnątrz jednoznacznie perkusyjnego folderu, aby nie klasyfikować losowych ścieżek.
    if (containsAny(normalisedName, { "l", "r" })
        && containsAny(normalisedGroup, { "drum", "drums", "cymbal", "cymbals", "overhead" }))
        return TrackFamily::cymbals;

    return classifyNormalised(normalisedGroup);
}

TrackRelationship classifyTrackRelationship(const TrackIdentity& first,
                                             const TrackIdentity& second)
{
    const auto firstName = normalise(first.name);
    const auto secondName = normalise(second.name);
    const auto firstFamily = classifyTrackFamily(first.name, first.group);
    const auto secondFamily = classifyTrackFamily(second.name, second.group);

    if (firstFamily == TrackFamily::effect || secondFamily == TrackFamily::effect)
    {
        if (firstFamily != secondFamily && !first.group.empty() && first.group == second.group)
            return TrackRelationship::sourceAndEffect;
    }

    if (firstFamily == secondFamily)
    {
        if ((firstFamily == TrackFamily::kick || firstFamily == TrackFamily::snare
             || firstFamily == TrackFamily::toms)
            && (hasCaptureHint(firstName) || hasCaptureHint(secondName)))
            return TrackRelationship::sameSourceMicrophones;

        if (firstFamily == TrackFamily::cymbals)
        {
            const auto bothOverheads = containsAny(firstName, { "oh", "overhead", "overheads" })
                                    && containsAny(secondName, { "oh", "overhead", "overheads" });
            return bothOverheads ? TrackRelationship::sameSourceMicrophones
                                 : TrackRelationship::relatedMicrophones;
        }

        if (firstFamily == TrackFamily::drumRoom)
            return TrackRelationship::sameSourceMicrophones;
    }

    const auto sameGroup = !first.group.empty() && first.group == second.group;
    if (sameGroup && isDrumCaptureFamily(firstFamily) && isDrumCaptureFamily(secondFamily)
        && (firstFamily == TrackFamily::drumRoom || secondFamily == TrackFamily::drumRoom))
        return TrackRelationship::relatedMicrophones;

    return TrackRelationship::independent;
}
} // namespace mixdebugger::analysis
