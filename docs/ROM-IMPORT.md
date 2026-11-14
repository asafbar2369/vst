# JD-800 wave ROM import — what is implemented, what was measured, what is assumed

Scope: the plug-in can play the JD-800's own multisamples **if the user supplies the instrument's three
wave-ROM chip dumps from their own disk**. This page says exactly what the code does, how each statement was
checked, and what is still unknown. Nothing here claims a hardware comparison: none was possible.

## 1. Legal handling

* **Nothing is bundled, copied or uploaded.** No ROM, firmware, factory bank, image or text from Roland is in this
  repository, in the VST3 or in the installer. The importer only reads files the user points it at, decodes them into
  memory, and never writes the audio anywhere. The project file stores the *path* only (`jdRomSource`).
* The CI runs and unit tests use a **synthetic ROM** (`Tests/JdRomFixture.h`) generated from sine maths in the chip's
  layout. The user's real dumps were used only in a local, isolated analysis session and are not in the repository.
* The D-50 firmware / PCM ROM and the JD-800 program ROM (`…ic31`, CRC-32 04565c05) are **not** read by anything.
* Sources consulted (format knowledge only; **no code copied**):
  * MAME machine data for `jd800` / `d50`: chip file names, sizes and CRC-32 values (factual metadata).
  * The `srscramble` repository README (the address / data line permutation of Roland's scrambled ROMs). That repository
    has **no licence file**; only the permutation table (a fact about wiring) was used and it was re-verified here
    (section 3, "descrambling"). The scrambler's code was not used.
  * An emulator of the JV-880 (Nuked-SC55 / MAME-style licence: **non-commercial**) was read for orientation about the
    chip family. Its code is **not** used; the sample-table layout below was re-derived and re-measured on the user's own
    dumps. If this plug-in is ever distributed commercially, have the importer reviewed again for this point.
  * Roland's JD-800 Owner's Manual II (waveform list: names, numbers, loop types).
* JV-880 / SR-JV80 / other ROM sets: not used.

## 2. What the user does

MIXER & FILES page → "JD-800 wave ROM (your own dumps)" → **ROM folder…** or **ROM .zip…** → the folder / zip that holds
the three chip files (any file names; only files of exactly 1 MiB are looked at, three folder levels deep). The plug-in
reports how many of the 108 waveforms were read, which chips were recognised by checksum, and any problem. **Use built-in**
returns to the substitutes. A waveform loaded from a user WAV file keeps priority over the ROM.

## 3. How the chips are read — and how each statement was checked

Classification: **verified** = checked against an independent fact; **measured** = measured on the user's dumps (local run,
reproducible with `DIGITALCOMBO_JD800_ROM_DIR=<folder> DigitalComboJdRomTest`); **inferred** = consistent with the
data, not provable without the hardware; **unknown** = not interpreted.

| Item | Status | Evidence |
|---|---|---|
| The three dumps are the published ones | verified | CRC-32 d743cec8 / 06098658 / 5c83e539 (MAME list) match the user's files exactly |
| Descrambling (address + data line permutation) | verified | after the permutation each chip starts with readable text (`1126_1`…, date `1990-11-26`) and self-consistent pointers; a wrong permutation cannot produce that |
| Header: +0x60 sample count, +0x62 group count, +0x80 sample table, +0x84 group table | measured | counts 201/246/212 samples and 37/36/35 groups; all pointers land on well-formed tables |
| 37 + 36 + 35 = **108** groups, names = the 108 JD-800 waveform names | verified | all 108 names match the manual's waveform list (spellings "Tabla", "ZZZ Vox", "Pole lp", "Brusky lp" differ from the earlier transcription; both are matched) |
| Group = 12-char name, 16 upper key limits, 16 sample numbers; last sample plays above the last limit | measured | e.g. "Pluck Harp": limits 59 67 71 77 85, samples 0…5 |
| Sample record: start (3 bytes), length (4), loop length (2), loop mode (1), root key (1) | measured | see next rows |
| Loop mode 2 = one shot | verified | **50 of 50** samples with mode 2 belong to waveforms the manual lists as one-shot (O); across all 659 samples, 0 disagreements between ROM loop flag and the manual's O/S/L column |
| Loop = last `loop length` samples | measured | the step across the loop seam is a typical sample-to-sample step (median 1.3–1.7 second differences) versus 7.4 for loops with a shifted start; loop length matches the measured pitch period (e.g. 245 vs 244, 115 vs 115, 76 vs 77) |
| Data = signed byte deltas × 2^shift, one 4-bit shift per 16 bytes from the table in the first 1/32 of the chip, accumulated | measured | decoded samples are strongly periodic (autocorrelation 0.91–0.96) and sample data starts exactly at 0x8000, the end of that table |
| Data rate 44.1 kHz | inferred | loop-derived pitch of long, high-N loops agrees with the root key to about ±10 cents at 44.1 kHz; short loops are too coarse to be more precise |
| Root key (byte 11), MIDI note (60 = middle C) | measured | the label alone leaves a pitch spread of 38 cents (max 103) against the sample's real pitch, see the next row |
| Tuning word: fields at +12 and +14 | **inferred (empirical fit)** | the sample's real pitch minus its label, in cents, is `0.1024 × (2090 − field12 − field14)`. Fit on 181 samples whose loop holds 1–3 periods (loop-length pitch, rounding below 6 cents): residual 2.9 cents, max 6.4. Checked against a *different* measurement (autocorrelation pitch of 89 samples, incl. 29 whole-sample loops and 4 one-shots): residual 9.7 cents (loops 10.9, whole-sample loops 7.4, one-shots 4.5), versus 38 cents spread without it. The importer applies it to each sample's root pitch. The mechanism in the hardware is not known; only the outcome (samples sit on their keys) is |
| Full scale = ±65536 accumulator | inferred | decoded peaks: median 37952, 90 % below 59648, max 65536 |
| DC handling (subtract loop / tail mean, 16-sample fade-in if needed) | **our approximation** | the original chip's accumulator drift is not modelled |
| Byte 0 of a sample record | **unknown** | tested as a level (gain from it makes neighbouring zones less, not more, even): no consistent relation found; not used |
| Table at +0x8c (216-byte records, one per group) | **unknown** | not used |

## 4. What the plug-in does with it (and what it does not)

* Each of the 108 waveforms becomes a list of key zones; the zone is chosen when the key is pressed and kept for the
  whole note; pitch = key relative to the sample's root key, resampled with the same band-limited player as the other
  sampled waves (no aliasing when transposed up).
* Loops and one-shots follow the ROM flags. TVF, TVA, envelopes, LFOs and effects are the plug-in's existing JD-800 model.
* **Not** done / not verifiable: a comparison with a hardware recording (none is available); the exact analog/digital
  signal path of the instrument after the wave generator; the level byte (unknown, see above). The tuning word is an
  empirical model of the effect, not the hardware's formula.
* Chips missing from the folder: the waveforms on those chips stay on the built-in substitutes (reported).
* A dump with an unpublished checksum is accepted **only** if its structure validates; it is then reported as
  "structure checked" and never as "verified".

## 5. D-50

No D-50 PCM ROM was available, and none was requested. The D-50's chips are identified by `tools/verify_pcm_roms.py`
(recognition only). Decoding / multisample mapping for the D-50 is **not implemented**; the D-50 engine keeps its
substitutes (LA32-style synth partials, generated PCM recipes) and says so in the README and the verification report.

## 6. SysEx / patch data

Bulk dumps (`.syx`) for both instruments are imported through the editor's SysEx import (DT1, bulk dump, packet
checksums and address ranges are validated; unknown or damaged messages are reported and skipped, never applied).
Factory patch banks are not bundled. Roland's D-50 factory-patch SysEx files carry Roland's licence terms and were not
downloaded; the user can import their own copies.

## 7. Tests

* `DigitalComboJdRomTest` (portable, no JUCE): CRC-32, scramble/descramble round trip and the exact bit mapping, table
  validation (every rejection path), delta decoding against the original waveform (max error 1.2e-4 of full scale),
  unaligned sample starts, DC removal, bounded output on random data, name mapping, zone selection incl. inclusive limits,
  priority of a user WAV file, partial chip sets, duplicate waves, rejection of random data / wrong sizes.
* `DigitalComboTests` → "JD-800 wave ROM import": scrambled synthetic chip files in a folder and in a `.zip`, pitch at
  8 keys over 4 octaves (within 12 cents), different samples per zone (levels 8000 : 16000 : 30000), inclusive zone limits,
  sine vs built-in saw spectrum, one-shot behaviour, second chip, project state size (no ROM data), restore of the
  location, WAV priority, rejection of noise / missing / empty locations, clearing while a note is held.
* With the user's real dumps (local only): the three chips are recognised, 108 / 108 waveforms are filled in about 110 ms,
  659 samples decode to finite values inside ±1.0 full scale, every loop lies inside its sample, loop flags agree with the
  manual in all 659 cases, loop seams are continuous, and with the tuning word the 188 loop-length pitches sit on their
  root keys (mean 0.7 cents, spread 2.9, worst 7.1). Through the whole plug-in (each of the 108 waveforms played at middle C):
  no NaN, every waveform sounds, every looped waveform keeps sounding while the key is held, all 17 one-shot waveforms die
  away. In CI this part is reported as **SKIP**, never as PASS.
