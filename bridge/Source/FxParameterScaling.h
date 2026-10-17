#pragma once

#include <functional>
#include <optional>

namespace mixdebugger::reaper_bridge
{
// Znajduje wartość 0..1 odpowiadającą żądanej wartości wyświetlanej przez FX.
// Jest potrzebne dla parametrów VST, których skala bywa nieliniowa i nie może
// być traktowana jak surowe dB/ms/ratio.
[[nodiscard]] std::optional<double> findClosestNormalizedValue(
    double targetDisplayedValue,
    const std::function<std::optional<double>(double)>& formatNormalizedValue);
} // namespace mixdebugger::reaper_bridge
