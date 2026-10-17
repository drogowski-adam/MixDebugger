# REAPER integration

The Windows extension adds **Extensions > MixDebugger: compare mix with reference...**.
It creates a local snapshot of the master output and opens that file in the
standalone app. Load the reference in MixDebugger, outside your project's FX chain.

## Install

Build and run `MixDebuggerSetup.exe` with REAPER closed, then restart REAPER.
The installer writes the app under `%LOCALAPPDATA%/Programs/MixDebugger` and the
extension under `%APPDATA%/REAPER/UserPlugins`. It installs no EQ or compressor.
For portable REAPER, copy `reaper_mixdebugger.dll` into that installation's
`UserPlugins` directory. The extension resolves the app through the install
registry entry, or the `MIXDEBUGGER_APP_PATH` environment variable for development.

## Render contract

- Current project, current master signal including its FX and fader.
- Current time selection, or the entire project if there is no selection.
- Stereo 48 kHz, 32-bit floating-point WAV; floating point retains above-zero peaks.
- No normalisation, limiting, dither, added project items or render tail.
- Existing mute/solo state is honoured. The extension does not change it.
- Temporary render settings are restored by scope-based cleanup, including errors.
- Each invocation uses a fresh cache directory, avoiding reuse of an old render.
- The action invokes REAPER's render operation; it does not analyse audio live.

The app opens with `--mix-file "absolute path"`. It does not accept commands that
apply changes to tracks. The bridge imports no gain, pan, polarity or FX setters.
The previous apply/undo IPC protocol is not registered or handled.

Renders are stored under `%LOCALAPPDATA%/MixDebugger/Bridge`. They can be removed
when no longer needed. Cache cleanup is currently manual. Ranges shown in the
app are relative to the rendered file, not absolute REAPER timeline time.

After editing the mix, run the menu action again and analyse the new file.
Results are snapshots; the app does not monitor subsequent REAPER changes.
A render sent while analysis is busy is rejected with a message; finish or cancel
analysis before retrying the REAPER action.

## Limitations and validation

REAPER may display its render progress window. Missing output and snapshots whose
decoded duration differs from the requested range by more than 20 ms are rejected.
External hardware inserts and non-deterministic FX
retain their normal REAPER rendering limitations. Device sharing depends on the
chosen audio driver.

See [validation checklist](validation.md) for the required manual REAPER smoke
checks. A successful compile is not proof that every REAPER configuration works.

API reference: https://www.reaper.fm/sdk/reascript/reascripthelp.html
