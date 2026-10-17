# Architecture

## Active application

- `app/Source/Main.cpp`: JUCE application lifecycle.
- `app/Source/ReferenceComponent.*`: file/range selection, worker lifecycle,
  comparison view, graphs, report export and playback.
- `core/include/mixdebugger/analysis/ReferenceAnalysis.h` and
  `core/src/ReferenceAnalysis.cpp`: streaming measurements, comparison text and
  matching gains. Files are read in approximately 100 ms blocks; entire songs
  are not loaded into RAM. Timeline storage grows with duration.
- `bridge/Source/BridgeExtension.cpp`: Windows REAPER extension; render master
  snapshot then launch the app with `--mix-file "path"`.
- `installer/`: app and extension installer; no correction FX payloads.

Analysis runs on a worker thread. Completion is posted to the message thread
using a JUCE SafePointer. Cancellation is checked at each input block; shutdown
cancels and joins the worker. Source/range edits invalidate measurements.
Playback uses a read-ahead thread and JUCE AudioTransportSource for device-rate
conversion. Audio never travels from the app back to REAPER.

## Previous prototype

The original stem-advisor sources and regression tests are retained to preserve
existing work. `MainComponent.*`, `PairAnalysisView.*`, bridge action-protocol and
FX helper files are not compiled into the new application/extension. Some legacy
DSP helpers are still compiled into the core static library for their regression
tests; the new UI does not call them. The optional VST probe is a historical
experimental target and is not required by this product.

There is no apply/undo protocol in the active bridge. It imports no track/FX
mutation functions. Removing a button alone would not have provided this property.

## Dependencies

CMake FetchContent pins JUCE 9.0.0, Catch2 3.15.3, libebur128 1.2.6 and a REAPER
SDK commit. Initial configuration needs network access. Subsequent builds use
the local dependency cache. Dependencies are statically linked where configured.

## Future work

Potential extensions include waveform-based range selection, click-free
crossfades for A/B transitions, richer timeline inspection, session persistence
and macOS/Linux packaging. These are not current features.
