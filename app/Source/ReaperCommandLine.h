#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace mixdebugger
{
[[nodiscard]] std::optional<juce::File> findReaperProjectManifest(
    const juce::String& commandLine);
}
