#pragma once

namespace mixdebugger::analysis
{
// Rola opisuje intencję aranżacyjną, a nie rodzaj instrumentu. Dzięki temu
// algorytm wie, która grupa ma pozostać czytelna, niezależnie od nazw ścieżek.
enum class MixRole
{
    unassigned,
    focus,
    foundation,
    support,
    background,
    ignore
};
} // namespace mixdebugger::analysis
