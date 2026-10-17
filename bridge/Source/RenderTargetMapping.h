#pragma once

#include <filesystem>
#include <optional>

namespace mixdebugger::reaper_bridge
{
[[nodiscard]] std::optional<int> trackNumberFromRenderPath(
    const std::filesystem::path& path);
}
