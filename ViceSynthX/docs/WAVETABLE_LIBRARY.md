# Factory Wavetable Library (500 tables)

All 500 factory wavetables are computed by code in `Source/Synth/WavetableFactory.cpp` from first principles
(harmonic series, two-operator FM, formant resonances, wavefolding, hard sync, phase maps, quantisation, comb
notches, drawbar registrations, seeded spectral clusters). No sampled audio, no third-party wavetable files and no
ROM data are used. Tables are generated on first use and cached (about 0.6 s for all 500 frame sets on the test
machine; a project only generates the tables it uses).

| Category | Tables | What changes across the table (32 frames) |
|---|---:|---|
| Classic | 27 | The original hand-designed tables (unchanged; existing presets sound the same) |
| Spectra | 75 | 5 rolloffs x 5 harmonic sets (full, odd, even-boost, every third removed, thirds boost) x 3 motions (opening low-pass from 6 harmonics, tilt, moving resonant peak with a trajectory per rolloff) |
| FM | 47 | 12 integer carrier:modulator ratios x 4 index curves (rise, curve, swell, deep); 1:1 Rise is left out (it is the classic "Digital FM") |
| Formant | 36 | Vowel glides between A, E, I, U x 3 registers (98, 165, 262 Hz); formant bandwidths grow with the register |
| Fold | 28 | Triangle folder over sine / triangle inputs x 4 drives x 3 offsets, plus a smooth ramp input x 4 drives |
| Sync | 36 | Saw / sine / square slave x 4 ratio ranges x raw / faded / half window |
| Pulse | 24 | 4 duty paths x hard / soft / rounded edges x single / dual |
| Warp | 16 | Sine and triangle through 8 phase maps (bend up / down, mirror, asymmetry, quantise, sine warp, pinch, stretch) |
| Stepped | 12 | Bit depth or sample-and-hold reduction of sine / triangle / saw, raw or low-passed |
| Comb | 16 | Notch patterns (spread, three, five, phaser) in saw / square spectra, light (-14 dB) or deep notches |
| Organ | 24 | 12 morphs between drawbar registrations, with or without key click |
| Cluster | 68 | Seeded groups of neighbouring harmonics moving through four key spectra |
| Texture | 12 | Three tilts x four spectral bands with moving partial phases |
| Partials | 58 | One harmonic gliding into another (1-10), and a fundamental joined by a rising harmonic |
| Ring | 21 | Harmonic a times harmonic b (sum and difference partials) |

## Distinctness check (Tests/TestWavetableLibrary.cpp)
- Fingerprint of a table: level in dB (relative to the frame's strongest harmonic, floor -60 dB) of harmonics 1-64 in
  four frames (start, 1/3, 2/3, end): 256 values. Two tables are near-duplicates when the RMS difference of their
  fingerprints is below **3 dB**.
- Result: 124,750 pairs, **0 below 3 dB**; closest pairs 3.05 dB (Organ Soft to Flute / Partials Duo 1+2), 3.06 dB,
  3.18 dB, 3.31 dB, 3.34 dB. Every table is finite and not silent; names are unique; every table has a category.
- How the families got there: the first design (473 generated tables) had **125 pairs below 3 dB**. They were fixed by
  design, not by changing the limit: the Stepped "Smooth" variant became a real low-pass; "Both" stepping, the saw
  base of Warp, the ramp offsets of Fold, FM 1:1 Rise, the vowel O (close to A and U) and the 440 Hz "Child" register
  (too few harmonics to keep vowels apart) were removed; Texture seeds were given separated bands; Comb "Light"
  notches deepened; Spectra emphasis and peak trajectories spread. Partials and Ring families replaced the removed
  tables.
- Limits of the check: it compares magnitudes of the first 64 harmonics only (phase differences and content above
  harmonic 64 are not counted), and 3 dB RMS is an engineering threshold, not a listening test. Many tables in a family
  share a character by design.

## In the instrument
The oscillator waveform menu and the resource menu list the factory tables in one submenu per category. Presets and
projects store tables by name (`factory:<name>`), so the 27 classic tables keep their names. The AI sound designer is
told the classic names, each family with examples and the total; any exact library name is accepted.
