#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mixdebugger::reaper_bridge
{
enum class MixActionKind
{
    gainDb,
    togglePolarity,
    offsetSeconds,
    stereoWidth,
    staticEq,
    compressor,
    stockEqPreset,
    stockCompressorPreset,
    saturation
};

struct MixAction
{
    MixActionKind kind = MixActionKind::gainDb;
    std::string guid;
    double value = 0.0;
    double secondValue = 0.0;
    double thirdValue = 0.0;
    double fourthValue = 0.0;
    std::string textValue;
};

[[nodiscard]] std::optional<std::vector<MixAction>> parseMixActionRequest(
    std::string_view contents);
} // namespace mixdebugger::reaper_bridge
