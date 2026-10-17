# Contributing

Use the README build instructions and run CTest before proposing changes.
Explain the user-visible behaviour and include relevant validation in a pull
request. Keep changes scoped; use synthetic or redistributable test audio.

Measurement changes need tests with independently known values or invariants
(gain shifts, polarity inversion, silence, channel count, sample-rate changes).
Avoid tests that merely duplicate implementation formulas. Document approximations
and unsupported cases instead of describing an estimate as certified accuracy.

The product is analysis-only. Do not add track/FX writes, automatic corrections,
or universal quality scores to the REAPER integration. Matching belongs solely
to playback. Keep blocking work off the UI and audio callback threads.

Bug reports should include app/REAPER versions, operating system, input format,
sample rate, selected ranges, expected behaviour and steps to reproduce. Include
audio only when you have permission to share it. No audio is uploaded by the app.

The project license is pending an author decision; resolve this before accepting
external contributions under an assumed open-source license.
