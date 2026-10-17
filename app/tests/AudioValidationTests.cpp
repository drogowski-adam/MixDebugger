#include "AudioValidation.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <optional>

TEST_CASE("an empty peak set is not classified as a silent render")
{
    REQUIRE_FALSE(mixdebugger::renderedAudioIsSuspiciouslySilent({}));
}

TEST_CASE("a render containing only silence and noise-floor peaks is rejected")
{
    const std::array<std::optional<float>, 4> peaks {
        std::nullopt,
        -std::numeric_limits<float>::infinity(),
        -93.2f,
        mixdebugger::minimumUsablePeakDb
    };

    REQUIRE(mixdebugger::renderedAudioIsSuspiciouslySilent(peaks));
}

TEST_CASE("one usable track makes a rendered set valid")
{
    const std::array<std::optional<float>, 3> peaks { -120.0f, -24.0f, -96.0f };

    REQUIRE_FALSE(mixdebugger::renderedAudioIsSuspiciouslySilent(peaks));
}
