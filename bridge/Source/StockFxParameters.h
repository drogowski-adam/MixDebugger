#pragma once

namespace mixdebugger::reaper_bridge
{
struct TimeAdjustmentParameters
{
    double delayMilliseconds = 0.0;
    double additionalSamples = 0.0;
    double totalSamples = 0.0;
};

// Składa istniejące ustawienie stockowego Time Adjustment Delay z kolejną,
// względną korektą. Wynik zachowuje dokładność pojedynczej próbki.
[[nodiscard]] TimeAdjustmentParameters composeTimeAdjustment(
    double currentDelayMilliseconds,
    double currentAdditionalSamples,
    double correctionSeconds,
    double sampleRate) noexcept;

struct StereoWidthParameters
{
    double widthBoostDb = 0.0;
    double centerBoostDb = 0.0;
    double outputGainDb = 0.0;
    double widthBalancePercent = 0.0;
    double rotationDegrees = 0.0;
    double totalSideMultiplier = 1.0;
};

// Parametry stockowego JS: Stereo Width [Stillwell]. Kompensacja Gain utrzymuje
// Mid na poziomie wejściowym, a Side otrzymuje dokładnie totalSideMultiplier.
[[nodiscard]] StereoWidthParameters composeStereoWidth(
    double currentCenterBoostDb,
    double relativeSideMultiplier,
    bool updatingManagedInstance) noexcept;
} // namespace mixdebugger::reaper_bridge
