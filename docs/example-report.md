# MixDebugger comparison

> Example generated from two synthetic 8-second test signals. These values do
> not describe a real song or a recommended mix target.

Material: Mix / pre-master

Mix: mix.wav (0.00-8.00 s, 48000 Hz, 2 channels)

Reference: reference.wav (0.00-8.00 s, 48000 Hz, 2 channels)

## What differs

- Your mix is 9.0 LU louder than the reference at original levels.
- Largest tonal difference: Bass occupies 48.0 percentage points more of the measured spectrum in your mix.
- Peaks stand 0.8 dB closer to the average level in your mix.
- Your mix has 1.7 percentage points less side energy (a stereo-width indicator).
- Matched playback gain: mix -9.0 dB, reference 0.0 dB. Original measurements stay unchanged.

## Measurements

| Metric | Mix | Reference | Mix minus reference |
|---|---:|---:|---:|
| Integrated loudness | -22.0 LUFS | -31.0 LUFS | 9.0 |
| Loudness range | 1.0 LU | 1.0 LU | 0.0 |
| Sample peak | -14.6 dBFS | -24.0 dBFS | 9.4 |
| True peak | -14.6 dBTP | -23.9 dBTP | 9.4 |
| Peak headroom | 14.6 dB | 23.9 dB | -9.4 |
| RMS | -23.4 dBFS | -33.6 dBFS | 10.2 |
| Crest factor | 8.8 dB | 9.6 dB | -0.8 |
| L/R correlation | 1.0 | 1.0 | 0.0 |
| Side energy | 0.6 % | 2.3 % | -1.7 |
| Mono energy change | -0.0 dB | -0.1 dB | 0.1 |

## Tonal balance

Band shares are level-independent, relative to total energy from 20 Hz to 20 kHz (or Nyquist). Differences are percentage points.

| Band | Mix | Reference | Difference |
|---|---:|---:|---:|
| Sub (20-60 Hz) | 0.0 % | 0.0 % | 0.0 |
| Bass (60-200 Hz) | 82.6 % | 34.6 % | 48.0 |
| Low mids (200-500 Hz) | 0.0 % | 0.0 % | 0.0 |
| Mids (500-2000 Hz) | 16.3 % | 61.5 % | -45.2 |
| Presence (2000-6000 Hz) | 0.0 % | 0.0 % | -0.0 |
| Air (6000-20000 Hz) | 1.0 % | 3.8 % | -2.8 |

## Stereo by band

| Band side energy | Mix | Reference | Difference |
|---|---:|---:|---:|
| Sub | 0.0 % | 0.0 % | -0.0 |
| Bass | 0.0 % | 0.0 % | -0.0 |
| Low mids | 0.1 % | 1.0 % | -0.9 |
| Mids | 2.2 % | 2.2 % | -0.0 |
| Presence | 11.6 % | 15.2 % | -3.5 |
| Air | 23.0 % | 23.0 % | -0.0 |

## Moments to compare

Times are relative to each selected passage. Loudness history uses original levels.

- Mix: loudest 400 ms window ends at 0.8 s (-19.2 LUFS).
- Mix: loudest 3 s window ends at 3.4 s (-21.4 LUFS).
- Reference: loudest 400 ms window ends at 5.8 s (-28.2 LUFS).
- Reference: loudest 3 s window ends at 5.9 s (-30.5 LUFS).

## Reading the results

LUFS describes perceived programme level. LRA describes longer-term variation; short excerpts may not represent a song. Crest factor describes peak-to-average contrast, not a compressor diagnosis. Side energy describes stereo distribution, not quality. Mono change compares (L+R)/2 energy with average L/R energy; negative values mean energy loss. N/A means unavailable, including silence or complete mono cancellation.

These are observations, not quality scores or mastering targets. Different arrangements can explain differences. Select comparable passages; no automatic section alignment is performed. Loudness matching only affects playback.
