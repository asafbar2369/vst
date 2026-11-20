# Factory Library QA

Status: **MEASURED** for the 85 factory presets in `Source/Presets/FactoryPresets.cpp` (log: `docs/qa/linux-test-run-9104fdb.log`).

## Inventory (verified from source)

- Presets: **85**, all original, built in code from the recipe library (`Source/Presets/SoundRecipes.cpp`).
- Categories (count): Analog (5), Bass (5), Bells (5), Brass (5), Choirs (5), Cinematic (5), Digital (5), Keys (5), Leads (5), Pads (5), Plucks (5), Sequences (5), Sound Effects (5), Strings (5), Textures (5), Vintage 1980s (5), Vintage 1990s (5)
- Development target from the brief: >= 2,000 presets in 24 categories, >= 500 wavetables. **Not met** (see FEATURE_GAP_ANALYSIS.md).
  The brief also forbids padding the count with trivial variations, so growth must come from new, quality-checked sounds.

## Checks performed (automated, every preset, rendered as played)

| Check | Method | Result |
|---|---|---|
| Renders, finite output | C4 (bass presets C2), velocity 100, held max(2 s, attack + 1.5 s), 1 s tail, arpeggiator / sequencer active | 85 / 85 pass |
| Loudness | RMS of the loudest 400 ms window (mean channel power) | 82 presets within -16 dB +/- 3; 3 transient presets peak-limited (crest factor 12-15 dB) |
| Peaks | sample peak | all <= -4.0 dBFS |
| Silence | loudest window < -45 dBFS | none |
| Near-duplicates | normalised parameter distance < 0.5 or acoustic similarity > 0.985 | 14 candidate pairs flagged for listening review (none acoustically identical; highest similarity 0.82) |
| Serialisation | every preset JSON round trip (TestCoreDSP) | 85 / 85 pass |

### Calibration defect found and fixed in this release
The previous calibration rendered every preset with the arpeggiator switched off, so arpeggio / sequence presets were never
measured as heard (Gated Chord Seq peaked at +9.1 dBFS, Kalimba Arp at +0.5 dBFS). The calibration now uses the as-played
method above; trims were recomputed from measurements (three passes until stable).

### Not yet checked (open)
- Velocity and mod-wheel response per preset, macro usefulness per preset, keyboard-range behaviour, CPU per preset.
- Subjective quality and categorisation review by listening (no listening test has been performed).
- Chords: a 6-note chord of a pad peaks near 0 dBFS (see KNOWN_LIMITATIONS.md).

### Library summary

| Measurement | Value | Unit |
|---|---:|---|
| presets rendered | 85.000 | count |
| level window min | -24.064 | dBFS |
| level window max | -15.786 | dBFS |
| highest peak | -3.897 | dBFS |
| near-duplicate candidate: Ocean Drive Poly / Sunset Boulevard Pad (param distance 0.32) | 0.202 | similarity |
| near-duplicate candidate: Ocean Drive Poly / Pop Synth Pad 84 (param distance 0.30) | 0.360 | similarity |
| near-duplicate candidate: Five Voice Brass / Synth Brass Stab (param distance 0.29) | 0.578 | similarity |
| near-duplicate candidate: Five Voice Brass / Eighties Poly Brass (param distance 0.48) | 0.400 | similarity |
| near-duplicate candidate: FM Tine Machine / Digital EP (param distance 0.21) | 0.025 | similarity |
| near-duplicate candidate: FM Tine Machine / Crystal EP 85 (param distance 0.32) | 0.027 | similarity |
| near-duplicate candidate: Sunset Boulevard Pad / Pop Synth Pad 84 (param distance 0.15) | 0.332 | similarity |
| near-duplicate candidate: Crystal Sky / Nineties Digital Pad (param distance 0.22) | 0.367 | similarity |
| near-duplicate candidate: Digital EP / Crystal EP 85 (param distance 0.19) | 0.817 | similarity |
| near-duplicate candidate: Soft Ooh / Dark Monks (param distance 0.37) | 0.076 | similarity |
| near-duplicate candidate: Brass Section / Horn Swell (param distance 0.07) | 0.802 | similarity |
| near-duplicate candidate: Synth Brass Stab / Eighties Poly Brass (param distance 0.24) | 0.559 | similarity |
| near-duplicate candidate: Crystal Mallets / Music Box (param distance 0.26) | 0.113 | similarity |
| near-duplicate candidate: Night Drive Sequence / Gated Chord Seq (param distance 0.11) | 0.231 | similarity |
| near-duplicate candidates | 14.000 | pairs |

### Per-preset level (as played) and peak

| Measurement | Value | Unit |
|---|---:|---|
| Ocean Drive Poly (Analog) loudest-400ms RMS | -15.786 | dBFS |
| Ocean Drive Poly peak | -4.581 | dBFS |
| Ladder Saw Stack (Analog) loudest-400ms RMS | -15.952 | dBFS |
| Ladder Saw Stack peak | -11.798 | dBFS |
| PWM Velvet (Analog) loudest-400ms RMS | -17.817 | dBFS |
| PWM Velvet peak | -9.881 | dBFS |
| Five Voice Brass (Analog) loudest-400ms RMS | -15.875 | dBFS |
| Five Voice Brass peak | -6.735 | dBFS |
| Diode Growler (Analog) loudest-400ms RMS | -15.950 | dBFS |
| Diode Growler peak | -9.547 | dBFS |
| Glass Harmonics (Digital) loudest-400ms RMS | -16.625 | dBFS |
| Glass Harmonics peak | -7.523 | dBFS |
| FM Tine Machine (Digital) loudest-400ms RMS | -16.024 | dBFS |
| FM Tine Machine peak | -7.069 | dBFS |
| Spectral Bloom (Digital) loudest-400ms RMS | -15.986 | dBFS |
| Spectral Bloom peak | -6.151 | dBFS |
| Bitcrush Arcade (Digital) loudest-400ms RMS | -16.006 | dBFS |
| Bitcrush Arcade peak | -5.167 | dBFS |
| Comb Morph Keys (Digital) loudest-400ms RMS | -16.501 | dBFS |
| Comb Morph Keys peak | -8.083 | dBFS |
| Ladder Mono Bass (Bass) loudest-400ms RMS | -15.934 | dBFS |
| Ladder Mono Bass peak | -9.252 | dBFS |
| Acid Line 303 (Bass) loudest-400ms RMS | -15.850 | dBFS |
| Acid Line 303 peak | -9.378 | dBFS |
| Reese Tide (Bass) loudest-400ms RMS | -16.021 | dBFS |
| Reese Tide peak | -12.329 | dBFS |
| FM Pluck Bass (Bass) loudest-400ms RMS | -15.986 | dBFS |
| FM Pluck Bass peak | -11.120 | dBFS |
| Neon Wobble (Bass) loudest-400ms RMS | -15.970 | dBFS |
| Neon Wobble peak | -10.998 | dBFS |
| Sunset Boulevard Pad (Pads) loudest-400ms RMS | -15.805 | dBFS |
| Sunset Boulevard Pad peak | -5.680 | dBFS |
| Crystal Sky (Pads) loudest-400ms RMS | -16.070 | dBFS |
| Crystal Sky peak | -7.148 | dBFS |
| Vowel Drift (Pads) loudest-400ms RMS | -16.561 | dBFS |
| Vowel Drift peak | -10.282 | dBFS |
| Angel Choir Pad (Pads) loudest-400ms RMS | -16.193 | dBFS |
| Angel Choir Pad peak | -6.754 | dBFS |
| Velvet Strings Pad (Pads) loudest-400ms RMS | -15.939 | dBFS |
| Velvet Strings Pad peak | -8.306 | dBFS |
| Miami Hero Lead (Leads) loudest-400ms RMS | -16.014 | dBFS |
| Miami Hero Lead peak | -7.767 | dBFS |
| Square Runner (Leads) loudest-400ms RMS | -15.999 | dBFS |
| Square Runner peak | -9.570 | dBFS |
| Sync Screamer (Leads) loudest-400ms RMS | -16.020 | dBFS |
| Sync Screamer peak | -6.987 | dBFS |
| FM Whistle (Leads) loudest-400ms RMS | -16.981 | dBFS |
| FM Whistle peak | -9.838 | dBFS |
| Korg Scream (Leads) loudest-400ms RMS | -15.989 | dBFS |
| Korg Scream peak | -10.918 | dBFS |
| Tine Electric Piano (Keys) loudest-400ms RMS | -16.030 | dBFS |
| Tine Electric Piano peak | -7.935 | dBFS |
| Felt Upright (Keys) loudest-400ms RMS | -16.008 | dBFS |
| Felt Upright peak | -5.528 | dBFS |
| Digital EP (Keys) loudest-400ms RMS | -15.973 | dBFS |
| Digital EP peak | -6.276 | dBFS |
| Funky Clav (Keys) loudest-400ms RMS | -16.024 | dBFS |
| Funky Clav peak | -5.862 | dBFS |
| Club Organ (Keys) loudest-400ms RMS | -16.035 | dBFS |
| Club Organ peak | -6.187 | dBFS |
| Palm Pluck (Plucks) loudest-400ms RMS | -15.929 | dBFS |
| Palm Pluck peak | -4.707 | dBFS |
| Tine Drops (Plucks) loudest-400ms RMS | -16.329 | dBFS |
| Tine Drops peak | -6.605 | dBFS |
| Kalimba Sun (Plucks) loudest-400ms RMS | -17.705 | dBFS |
| Kalimba Sun peak | -4.785 | dBFS |
| Nylon Guitar (Plucks) loudest-400ms RMS | -18.676 | dBFS |
| Nylon Guitar peak | -4.015 | dBFS |
| Nylon Guitar peak-limited (crest factor) | 14.661 | dB |
| Glass Pizz (Plucks) loudest-400ms RMS | -16.989 | dBFS |
| Glass Pizz peak | -5.654 | dBFS |
| Ensemble Section (Strings) loudest-400ms RMS | -16.170 | dBFS |
| Ensemble Section peak | -7.624 | dBFS |
| Solina Night (Strings) loudest-400ms RMS | -15.865 | dBFS |
| Solina Night peak | -5.952 | dBFS |
| Spectral Violins (Strings) loudest-400ms RMS | -17.299 | dBFS |
| Spectral Violins peak | -10.255 | dBFS |
| Staccato Strings (Strings) loudest-400ms RMS | -18.903 | dBFS |
| Staccato Strings peak | -5.365 | dBFS |
| Cinema Swell (Strings) loudest-400ms RMS | -15.845 | dBFS |
| Cinema Swell peak | -6.777 | dBFS |
| Cathedral Aah (Choirs) loudest-400ms RMS | -16.175 | dBFS |
| Cathedral Aah peak | -5.396 | dBFS |
| Soft Ooh (Choirs) loudest-400ms RMS | -16.099 | dBFS |
| Soft Ooh peak | -5.668 | dBFS |
| Synth Vox (Choirs) loudest-400ms RMS | -16.525 | dBFS |
| Synth Vox peak | -5.354 | dBFS |
| Breath Choir (Choirs) loudest-400ms RMS | -16.176 | dBFS |
| Breath Choir peak | -6.390 | dBFS |
| Dark Monks (Choirs) loudest-400ms RMS | -16.116 | dBFS |
| Dark Monks peak | -7.416 | dBFS |
| Brass Section (Brass) loudest-400ms RMS | -16.003 | dBFS |
| Brass Section peak | -5.737 | dBFS |
| Synth Brass Stab (Brass) loudest-400ms RMS | -15.927 | dBFS |
| Synth Brass Stab peak | -6.357 | dBFS |
| Wavetable Horns (Brass) loudest-400ms RMS | -16.049 | dBFS |
| Wavetable Horns peak | -6.146 | dBFS |
| Vangelis Blade (Brass) loudest-400ms RMS | -15.924 | dBFS |
| Vangelis Blade peak | -6.506 | dBFS |
| Horn Swell (Brass) loudest-400ms RMS | -16.020 | dBFS |
| Horn Swell peak | -5.272 | dBFS |
| FM Glass Bells (Bells) loudest-400ms RMS | -15.956 | dBFS |
| FM Glass Bells peak | -5.230 | dBFS |
| Crystal Mallets (Bells) loudest-400ms RMS | -16.018 | dBFS |
| Crystal Mallets peak | -6.021 | dBFS |
| Singing Bowl (Bells) loudest-400ms RMS | -16.008 | dBFS |
| Singing Bowl peak | -8.479 | dBFS |
| Tubular Night (Bells) loudest-400ms RMS | -16.692 | dBFS |
| Tubular Night peak | -5.679 | dBFS |
| Music Box (Bells) loudest-400ms RMS | -16.005 | dBFS |
| Music Box peak | -9.092 | dBFS |
| Granular Fog (Textures) loudest-400ms RMS | -16.038 | dBFS |
| Granular Fog peak | -4.935 | dBFS |
| Breathing Room (Textures) loudest-400ms RMS | -17.034 | dBFS |
| Breathing Room peak | -4.107 | dBFS |
| Breathing Room peak-limited (crest factor) | 12.926 | dB |
| Spectral Tides (Textures) loudest-400ms RMS | -16.281 | dBFS |
| Spectral Tides peak | -9.718 | dBFS |
| Lo-Fi Memory (Textures) loudest-400ms RMS | -17.076 | dBFS |
| Lo-Fi Memory peak | -8.013 | dBFS |
| Neon Rain (Textures) loudest-400ms RMS | -16.169 | dBFS |
| Neon Rain peak | -4.068 | dBFS |
| Neon Rain peak-limited (crest factor) | 12.102 | dB |
| Deep Space Drone (Cinematic) loudest-400ms RMS | -17.029 | dBFS |
| Deep Space Drone peak | -6.801 | dBFS |
| Trailer Impact (Cinematic) loudest-400ms RMS | -15.951 | dBFS |
| Trailer Impact peak | -4.986 | dBFS |
| Tension Riser (Cinematic) loudest-400ms RMS | -16.432 | dBFS |
| Tension Riser peak | -4.533 | dBFS |
| Tension Riser peak-limited (crest factor) | 11.899 | dB |
| Heroic Swell (Cinematic) loudest-400ms RMS | -16.179 | dBFS |
| Heroic Swell peak | -9.101 | dBFS |
| Vice City Dusk (Cinematic) loudest-400ms RMS | -15.843 | dBFS |
| Vice City Dusk peak | -6.004 | dBFS |
| Laser Zap (Sound Effects) loudest-400ms RMS | -18.001 | dBFS |
| Laser Zap peak | -7.645 | dBFS |
| Police Siren (Sound Effects) loudest-400ms RMS | -16.747 | dBFS |
| Police Siren peak | -4.010 | dBFS |
| Police Siren peak-limited (crest factor) | 12.737 | dB |
| White Noise Sweep (Sound Effects) loudest-400ms RMS | -17.466 | dBFS |
| White Noise Sweep peak | -6.672 | dBFS |
| Robot Talk (Sound Effects) loudest-400ms RMS | -16.622 | dBFS |
| Robot Talk peak | -3.964 | dBFS |
| Robot Talk peak-limited (crest factor) | 12.658 | dB |
| Arcade Coin (Sound Effects) loudest-400ms RMS | -24.064 | dBFS |
| Arcade Coin peak | -4.002 | dBFS |
| Arcade Coin peak-limited (crest factor) | 20.063 | dB |
| Night Drive Sequence (Sequences) loudest-400ms RMS | -15.990 | dBFS |
| Night Drive Sequence peak | -4.814 | dBFS |
| Arp Lights (Sequences) loudest-400ms RMS | -16.303 | dBFS |
| Arp Lights peak | -4.997 | dBFS |
| Bassline Engine (Sequences) loudest-400ms RMS | -15.806 | dBFS |
| Bassline Engine peak | -6.863 | dBFS |
| Kalimba Arp (Sequences) loudest-400ms RMS | -16.314 | dBFS |
| Kalimba Arp peak | -4.690 | dBFS |
| Gated Chord Seq (Sequences) loudest-400ms RMS | -16.022 | dBFS |
| Gated Chord Seq peak | -3.897 | dBFS |
| Gated Chord Seq peak-limited (crest factor) | 12.125 | dB |
| Eighties Poly Brass (Vintage 1980s) loudest-400ms RMS | -15.926 | dBFS |
| Eighties Poly Brass peak | -7.623 | dBFS |
| Crystal EP 85 (Vintage 1980s) loudest-400ms RMS | -15.990 | dBFS |
| Crystal EP 85 peak | -6.005 | dBFS |
| Synthwave Lead (Vintage 1980s) loudest-400ms RMS | -15.966 | dBFS |
| Synthwave Lead peak | -6.957 | dBFS |
| Pop Synth Pad 84 (Vintage 1980s) loudest-400ms RMS | -15.808 | dBFS |
| Pop Synth Pad 84 peak | -6.478 | dBFS |
| Gated Drum Synth (Vintage 1980s) loudest-400ms RMS | -17.106 | dBFS |
| Gated Drum Synth peak | -3.979 | dBFS |
| Gated Drum Synth peak-limited (crest factor) | 13.126 | dB |
| Nineties Digital Pad (Vintage 1990s) loudest-400ms RMS | -16.039 | dBFS |
| Nineties Digital Pad peak | -4.967 | dBFS |
| Rave Hoover (Vintage 1990s) loudest-400ms RMS | -15.956 | dBFS |
| Rave Hoover peak | -4.724 | dBFS |
| House Organ Bass (Vintage 1990s) loudest-400ms RMS | -16.000 | dBFS |
| House Organ Bass peak | -5.476 | dBFS |
| Trance Supersaw (Vintage 1990s) loudest-400ms RMS | -17.010 | dBFS |
| Trance Supersaw peak | -6.661 | dBFS |
| Pop Bell Pad 92 (Vintage 1990s) loudest-400ms RMS | -16.165 | dBFS |
| Pop Bell Pad 92 peak | -5.284 | dBFS |
