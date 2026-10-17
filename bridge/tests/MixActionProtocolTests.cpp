#include "../Source/MixActionProtocol.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Mix action protocol parses gain phase and timing changes")
{
    using namespace mixdebugger::reaper_bridge;
    const auto parsed = parseMixActionRequest(
        "MIXDEBUGGER_APPLY_V1\n"
        "gain\t__MASTER__\t-4.0\n"
        "gain\t{A}\t-2.5\n"
        "phase\t{B}\n"
        "offset\t{C}\t-0.0015\n"
        "width\t{F}\t0.55\n"
        "eq\t{D}\t2100\t-2.2\t1.1\n"
        "compressor\t{E}\t-24\t2.5\t20\t90\n"
        "stockeq\t{G}\tstock - Wide male vocal\n"
        "stockcomp\t{G}\tstock - Modern Vocal\t-24\t2.5\t18\t90\n"
        "saturation\t{G}\t7.0\n");
    REQUIRE(parsed);
    REQUIRE(parsed->size() == 10);
    CHECK((*parsed)[0].kind == MixActionKind::gainDb);
    CHECK((*parsed)[0].guid == "__MASTER__");
    CHECK((*parsed)[0].value == -4.0);
    CHECK((*parsed)[1].guid == "{A}");
    CHECK((*parsed)[1].value == -2.5);
    CHECK((*parsed)[2].kind == MixActionKind::togglePolarity);
    CHECK((*parsed)[3].kind == MixActionKind::offsetSeconds);
    CHECK((*parsed)[4].kind == MixActionKind::stereoWidth);
    CHECK((*parsed)[4].value == 0.55);
    CHECK((*parsed)[5].kind == MixActionKind::staticEq);
    CHECK((*parsed)[5].secondValue == -2.2);
    CHECK((*parsed)[6].kind == MixActionKind::compressor);
    CHECK((*parsed)[6].fourthValue == 90.0);
    CHECK((*parsed)[7].kind == MixActionKind::stockEqPreset);
    CHECK((*parsed)[7].textValue == "stock - Wide male vocal");
    CHECK((*parsed)[8].kind == MixActionKind::stockCompressorPreset);
    CHECK((*parsed)[8].textValue == "stock - Modern Vocal");
    CHECK((*parsed)[8].value == -24.0);
    CHECK((*parsed)[8].secondValue == 2.5);
    CHECK((*parsed)[8].thirdValue == 18.0);
    CHECK((*parsed)[8].fourthValue == 90.0);
    CHECK((*parsed)[9].kind == MixActionKind::saturation);
    CHECK((*parsed)[9].value == 7.0);
}

TEST_CASE("Mix action protocol rejects malformed or empty requests")
{
    using mixdebugger::reaper_bridge::parseMixActionRequest;
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_APPLY_V1\n"));
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_APPLY_V1\ngain\t{A}\twrong\n"));
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_APPLY_V1\ngain\t{A}\tnan\n"));
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_APPLY_V1\nsaturation\t{A}\t101\n"));
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_APPLY_V1\nstockeq\t{A}\t\n"));
    CHECK_FALSE(parseMixActionRequest(
        "MIXDEBUGGER_APPLY_V1\nstockcomp\t{A}\tstock - Modern Vocal\n"));
    CHECK_FALSE(parseMixActionRequest("MIXDEBUGGER_REQUEST_V1\nguid={A}\n"));
}
