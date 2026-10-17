#include "MixActionProtocol.h"

#include <charconv>
#include <cmath>
#include <sstream>
#include <system_error>
#include <utility>

namespace mixdebugger::reaper_bridge
{
namespace
{
std::vector<std::string_view> splitTabs(const std::string_view line)
{
    std::vector<std::string_view> result;
    auto begin = std::size_t{};
    while (begin <= line.size())
    {
        const auto end = line.find('\t', begin);
        result.push_back(line.substr(begin, end == std::string_view::npos
                                             ? std::string_view::npos : end - begin));
        if (end == std::string_view::npos)
            break;
        begin = end + 1;
    }
    return result;
}

bool parseNumber(const std::string_view text, double& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
        && std::isfinite(value);
}
}

std::optional<std::vector<MixAction>> parseMixActionRequest(const std::string_view contents)
{
    std::istringstream input { std::string(contents) };
    std::string line;
    if (!std::getline(input, line) || line != "MIXDEBUGGER_APPLY_V1")
        return std::nullopt;

    std::vector<MixAction> actions;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const auto fields = splitTabs(line);
        if (fields.size() < 2 || fields[1].empty())
            return std::nullopt;
        const auto operation = fields[0];
        const auto guid = std::string(fields[1]);

        if (operation == "phase")
        {
            if (fields.size() != 2)
                return std::nullopt;
            actions.push_back({ MixActionKind::togglePolarity, guid });
            if (actions.size() > 512)
                return std::nullopt;
            continue;
        }

        MixAction action;
        action.guid = guid;
        if (operation == "gain" || operation == "offset" || operation == "width")
        {
            if (fields.size() != 3 || !parseNumber(fields[2], action.value))
                return std::nullopt;
            if (operation == "gain")
                action.kind = MixActionKind::gainDb;
            else if (operation == "offset")
                action.kind = MixActionKind::offsetSeconds;
            else
                action.kind = MixActionKind::stereoWidth;
        }
        else if (operation == "eq")
        {
            if (fields.size() != 5 || !parseNumber(fields[2], action.value)
                || !parseNumber(fields[3], action.secondValue)
                || !parseNumber(fields[4], action.thirdValue))
                return std::nullopt;
            action.kind = MixActionKind::staticEq;
        }
        else if (operation == "compressor")
        {
            if (fields.size() != 6 || !parseNumber(fields[2], action.value)
                || !parseNumber(fields[3], action.secondValue)
                || !parseNumber(fields[4], action.thirdValue)
                || !parseNumber(fields[5], action.fourthValue))
                return std::nullopt;
            action.kind = MixActionKind::compressor;
        }
        else if (operation == "stockeq")
        {
            if (fields.size() != 3 || fields[2].empty())
                return std::nullopt;
            action.kind = MixActionKind::stockEqPreset;
            action.textValue = std::string(fields[2]);
        }
        else if (operation == "stockcomp")
        {
            if (fields.size() != 7 || fields[2].empty()
                || !parseNumber(fields[3], action.value)
                || !parseNumber(fields[4], action.secondValue)
                || !parseNumber(fields[5], action.thirdValue)
                || !parseNumber(fields[6], action.fourthValue))
                return std::nullopt;
            action.kind = MixActionKind::stockCompressorPreset;
            action.textValue = std::string(fields[2]);
        }
        else if (operation == "saturation")
        {
            if (fields.size() != 3 || !parseNumber(fields[2], action.value)
                || action.value < 0.0 || action.value > 100.0)
                return std::nullopt;
            action.kind = MixActionKind::saturation;
        }
        else
        {
            return std::nullopt;
        }
        actions.push_back(std::move(action));
        if (actions.size() > 512)
            return std::nullopt;
    }
    return actions.empty() ? std::nullopt
                           : std::optional<std::vector<MixAction>>(std::move(actions));
}
} // namespace mixdebugger::reaper_bridge
