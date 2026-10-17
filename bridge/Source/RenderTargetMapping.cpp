#include "RenderTargetMapping.h"

#include <string>
#include <string_view>

namespace mixdebugger::reaper_bridge
{
std::optional<int> trackNumberFromRenderPath(const std::filesystem::path& path)
{
    constexpr std::wstring_view prefix = L"mixdebugger-";
    const auto stem = path.stem().wstring();
    if (!std::wstring_view(stem).starts_with(prefix))
        return std::nullopt;

    const auto digits = std::wstring_view(stem).substr(prefix.size());
    std::size_t length = 0;
    while (length < digits.size() && digits[length] >= L'0' && digits[length] <= L'9')
        ++length;
    if (length == 0)
        return std::nullopt;

    try
    {
        return std::stoi(std::wstring(digits.substr(0, length)));
    }
    catch (...)
    {
        return std::nullopt;
    }
}
} // namespace mixdebugger::reaper_bridge
