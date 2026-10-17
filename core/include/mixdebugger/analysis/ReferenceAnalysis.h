#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <optional>
#include <vector>

namespace mixdebugger::analysis {
inline constexpr std::array<double, 7> bandEdges {20, 60, 200, 500, 2000, 6000, 20000};
inline constexpr std::array<const char*, 6> bandNames {"Sub", "Bass", "Low mids", "Mids", "Presence", "Air"};
struct TimePoint {
    double seconds = 0;
    std::optional<double> momentary, shortTerm;
    double peakDb = -150;
};
struct ReferenceResult {
    juce::String name;
    double start = 0, end = 0, sampleRate = 0;
    unsigned channels = 0;
    std::optional<double> integrated, range, peak, truePeak, rms, crest, correlation, sidePercent, monoLoss;
    std::array<double, 6> bandPercent {};
    std::array<double, 6> bandSidePercent {};
    bool hasSpectrum = false;
    std::vector<TimePoint> timeline;
};
// end == 0 means end of file. No audio files or project settings are modified.
ReferenceResult analyseReferenceFile(const juce::File&, double start, double end,
                                     const std::atomic<bool>& cancel);
// Attenuation only, with common headroom, so matching cannot clip either source.
std::optional<std::array<double, 2>> matchedGainsDb(const ReferenceResult&, const ReferenceResult&);
juce::String comparisonMarkdown(const ReferenceResult&, const ReferenceResult*, const juce::String& material);
}
