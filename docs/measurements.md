# Measurement definitions

## Loudness and peaks

Integrated loudness, LRA, momentary loudness, short-term loudness and true peak
are measured using **libebur128 1.2.6**. This avoids presenting an unweighted RMS
calculation as LUFS. Integrated loudness uses K-weighting and gating. LRA is
based on the gated distribution of short-term loudness, not crest factor.

History is sampled approximately every 100 ms. Momentary values require at
least 400 ms of selected audio; short-term values require three seconds. The
meter starts at the selected range boundary, so it has no pre-roll from the
preceding part of the file. Partial final blocks are included in audio statistics.

Mono is measured as one channel, not dual mono. Playback duplicates mono to both
output channels; matching accounts for the corresponding +3.0103 LU of channel
energy. Original mono LUFS results remain single-channel measurements.

- Sample peak: largest absolute stored sample, in dBFS.
- True peak: interpolated peak estimate from libebur128, in dBTP. It is not a
  guarantee for every downstream resampler or lossy codec.
- RMS: square root of average channel energy, expressed in dBFS.
- Crest factor: sample peak dBFS minus RMS dBFS over the selected passage.

No streaming-platform or genre loudness target is imposed.

## Tone

Independent left and right FFTs use 4096-sample Hann windows without overlap.
The final partial window is zero-padded. Channel power is averaged, avoiding
cancellation of out-of-phase stereo content before spectrum measurement.

| Band | Hz |
| --- | --- |
| Sub | 20–60 |
| Bass | 60–200 |
| Low mids | 200–500 |
| Mids | 500–2000 |
| Presence | 2000–6000 |
| Air | 6000–20000 |

Each share is its band energy divided by summed band energy. These are power
proportions, not perceptual loudness proportions. Differences are percentage
points, not dB boosts/cuts. Frequencies above Nyquist are absent; references with
different sample rates may have different available bandwidth. FFT resolution
depends on sample rate; narrow features near a band boundary can affect both
neighbouring bands. The analysis does not identify instruments or masking causes.

## Stereo and mono

`M=(L+R)/2`, `S=(L-R)/2`. Side energy is `100*S²/(M²+S²)`, with energy summed over
the passage. The same calculation is applied to complex FFT bins within bands.
Correlation is `sum(L*R)/sqrt(sum(L²)*sum(R²))`; no DC subtraction is performed.

Mono energy change is `10*log10(sum(M²)/((sum(L²)+sum(R²))/2))`.
Identical left/right channels give 0 dB, uncorrelated channels approximately
-3 dB, and opposite polarity cancels completely. Complete cancellation is
shown as N/A in the numeric table and explicitly described in the summary.

These are aggregate indicators. They cannot establish subjective width, detect
every local phase issue, or diagnose the cause of lost content.

## Matching

The target is the quieter of the two playback-adjusted loudness readings. Each gain is
`target - input loudness`, so no source is boosted. A shared further reduction
keeps both measured true peaks at or below -1 dBTP. Original results are retained.
Invalid/silent inputs disable matching. Device resampling can change peak values.

## References

- [libebur128](https://github.com/jiixyj/libebur128/tree/v1.2.6)
- [EBU loudness resources](https://tech.ebu.ch/loudness/)
- [EBU Tech 3342: Loudness Range](https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3342v2_0.pdf)

Automated tests cover calibrated tones and important invariants. This application
has not been certified as an EBU-compliant meter or mastering delivery validator.
