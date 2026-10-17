# User guide

## Choose comparable material

The material selector records whether you are examining a mix/pre-master or a
finished master. It does not alter the audio, change the measurement algorithm,
or impose a target LUFS value.

For a useful reference comparison, choose similar musical passages: a chorus
against a chorus, or two full tracks with similar arrangements. Ranges are
independent and expressed in file-relative seconds. There is no automatic
verse/chorus detection or beat alignment. A REAPER selection arrives as a new
file beginning at zero.

Analysis requires at least 400 ms. LRA is unavailable for passages shorter than
three seconds, and even longer short passages may not represent whole-track
dynamics. For reliable programme comparisons, prefer full tracks or substantial
sections.

## Read the comparison

The summary describes the largest measured differences. A greater bass share
does not establish that the bass is excessive. More side energy does not
establish that a mix is better. Arrangements, production choices and different
instruments all influence these measurements.

The tonal graph compares energy proportions, independent of overall gain. The
loudness graph shows **original levels** and a relative 0–100% passage axis.
Two points at 50% may describe different musical events. The graph clips its
display scale to -60 through 0 LUFS; measurements retain their actual values.

The detailed view contains the same Markdown as the exported report. N/A means
the measurement is unavailable, not zero. Silence has no usable integrated
loudness and cannot be loudness-matched.

## Listen

Play A starts the mix and Play B starts the reference. Switching uses the same
elapsed seconds relative to each selected start. If that position is past the
other passage's end, playback restarts at that passage's beginning. Stop ends
playback; the next Play starts at the beginning. Playback stops at the range end.

Matching attenuates the louder source to the quieter source's measured loudness.
Both receive additional common attenuation if needed for 1 dB of measured true
peak headroom. This is playback gain only. Matching cannot operate without two
valid loudness results. Unmatched playback uses original file levels.

The app uses the default output device. REAPER may need to release an exclusive
audio device while in the background. No audio input is required.

## Refresh and export

Changing a file or range invalidates the report and disables playback until a
new analysis is complete. Edits to the source file outside MixDebugger are not
monitored: analyse again after replacing files. Edits in REAPER require another
render via its Extensions menu, then a new analysis.

Export Markdown produces a portable text report with source filenames, ranges,
sample rates, metrics and explanations. Export does not embed audio.
