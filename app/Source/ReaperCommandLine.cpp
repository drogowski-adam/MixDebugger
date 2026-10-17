#include "ReaperCommandLine.h"

namespace mixdebugger
{
std::optional<juce::File> findReaperProjectManifest(const juce::String& commandLine)
{
    const auto arguments = juce::StringArray::fromTokens(commandLine, true);
    for (int index = 0; index < arguments.size(); ++index)
    {
        auto argument = arguments[index].trim();
        if (argument == "--reaper-project" && index + 1 < arguments.size())
            return juce::File(arguments[index + 1].trim().unquoted());

        constexpr auto prefix = "--reaper-project=";
        if (argument.startsWith(prefix))
            return juce::File(argument.substring(static_cast<int>(std::char_traits<char>::length(prefix)))
                                      .trim().unquoted());
    }
    return std::nullopt;
}
} // namespace mixdebugger
