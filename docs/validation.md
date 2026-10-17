# Validation

## Automated checks

Run the README CMake/CTest commands. The current suite includes eight new
reference-analysis cases alongside the retained prototype regression tests:

- Calibrated 1 kHz stereo tone: approximately -20 LUFS at -20 dBFS peak.
- Gain invariance of tonal/stereo descriptors and expected LUFS shift.
- Opposite polarity: retained spectral energy, negative correlation and mono loss.
- Silence: unavailable loudness, disabled matching.
- Independent range bounds and 44.1/96 kHz inputs.
- Single-channel convention and cancellation.
- Shared playback headroom with no gain boost.
- Mono-to-stereo playback compensation.

The installer supports `MixDebuggerSetup.exe --verify`, which checks embedded
payloads without installing them. Exit code zero indicates successful verification.

On 2026-10-17 the Windows Release build and all 96 CTest cases passed. Installer
payload verification returned zero. A CLI comparison of two synthetic WAVs
produced [example-report.md](example-report.md). Those fixtures are test signals,
not commercial recordings. The GUI was opened on the same files and its report,
graphs and playback progress were inspected.

These checks do not constitute EBU certification, a listening evaluation, or a
complete live REAPER integration test.

## Manual REAPER release checklist

- Install the new extension with REAPER closed and restart the host.
- Render a known stereo project and compare its output against a manual render.
- Test both full-project and time-selection ranges.
- Confirm a fresh render after changing master FX or master level.
- Confirm muted/soloed tracks behave as in a normal REAPER master render.
- Cancel a render; verify incomplete output is rejected.
- Check that render settings, track values and FX are unchanged afterwards.
- Test paths containing spaces and non-ASCII characters, including portable installs.
- Verify output-device sharing and A/B transitions with the intended driver.
- Compare LUFS, LRA and true peak against independent reference-meter test material.

This checklist remains required before describing the REAPER integration as
fully validated for release.
