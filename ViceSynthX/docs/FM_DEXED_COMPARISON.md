# FM engine vs Dexed "Modern" engine: measured equivalence (2026-11-21)

## Result
With identical patches and identical MIDI, VICE SYNTH X's FM renderer produces the same samples as Dexed's
**Modern** engine (MSFA `FmCore`, Dexed 1ed5a4d).

| Check | Cases | Result |
|---|---|---|
| In-process regression test (`Tests/TestDexedEquivalence.cpp`, runs on every CI build) | 57 | median residual **−267 dB** (most cases bit-exact); worst −58.8 dB; largest single difference **1 sixteen-bit LSB**; 172 checks passed, 0 failed |
| External harness built from a separate Dexed checkout (`tools/dexed_reference/`) | 57 | median residual **−117 dB**; worst −57.4 dB; gain difference **0.000 dB** in every case; constant 1-sample latency |

- **Residual** is the energy of (VICE SYNTH X − Dexed) relative to the energy of Dexed's output over 3 s: the note is held
  for 2 s, then released.
- The two worst cases are quiet random patches (peak about −60 dBFS). There, a single 1-LSB difference on the first
  sample is −57 to −59 dB relative to the signal. Everywhere else they match at −110 to −130 dB.
- The external harness applies Dexed's output DC blocker in float to both sides. That is why its median is −117 dB
  rather than bit-exact.
- Per-case numbers: `docs/measurements/fm_dexed/external_harness_results.csv`, and the `MEASURE|dexed|` lines of the
  test log.

## What was compared
- **Patches:**
  - all 12 factory FM voices at C4 / velocity 100 and at C2 / velocity 64;
  - 32 random voices, one per algorithm (random rates and levels, keyboard level and rate scaling, curves, velocity
    and amp-mod sensitivity, detune, coarse and fine, fixed-frequency operators, feedback 0–7, oscillator key sync,
    pitch EG, LFO speed / delay / PMD / AMD / PMS / waves, transpose);
  - a five-note chord.
- **Reference:** MSFA driven the way Dexed's `DexedAudioProcessor` drives it:
  - 16 voices; the global LFO reset on program load and triggered by the first key;
  - `oscSync()` on note-on when key sync is on; transpose applied on key-down;
  - pitch bend centred (range 3, Dexed's default);
  - per-voice `>> 4`, then `>> 9` to 16 bit with Dexed's overflow handling; voices summed; 64-sample blocks.
- **Device under test:** `fm::FmRenderer` at 48 kHz, one renderer per note, as the synth's voices use it.

## Independence and limits (read this)
- **Same engine source.** VICE SYNTH X *uses* MSFA: its files are vendored unmodified from Dexed, and the build was
  checked to be byte-identical to Dexed 1ed5a4d `source/msfa`. This comparison therefore validates the
  **integration**: patch encoding (VCED + operator switch), transpose, velocity, pitch, LFO handling, key sync, output
  conversion, block timing and resampling. It does not validate MSFA against hardware.
- **Dexed's default engine is "Mark I", not "Modern".** Mark I is GPLv3 code with a different level and feedback
  model, so it sounds different. To hear the same sound in Dexed, select **Engine: Modern** in Dexed's settings.
  Mark I is not used here: using it would require distributing the whole plug-in under GPLv3, which is an owner
  decision.
- **Known intended differences in the plug-in (outside this comparison):**
  - **FM Amount.** At its default 1.0 nothing changes. Other values scale modulator output levels.
  - **Pitch.** The synth's pitch (bend, glide, octave / semi, modulation) is passed as MSFA pitch bend with a ±48
    semitone range, at about 0.7 cent resolution. At centre pitch this is identical.
  - **LFO per voice.** Dexed has one global LFO. Here each voice has its own LFO, so chords with LFO modulation have
    independent LFO phases. The chord case has its LFO off for this reason.
  - **Negative overflow.** When a single voice exceeds full scale negatively, Dexed's conversion outputs **+1.0**: the
    sign is flipped, which produces a click. VICE SYNTH X outputs −1.0. Only voices louder than full scale are
    affected.
  - **Synth stages.** After the FM renderer, the synth's oscillator level (+12 dB in FM mode), envelopes, filters,
    effects and Preset Volume apply.
- **Note-off timing.** Both engines work in 64-sample blocks, so a note-off takes effect at a block boundary. The
  renderer computes one sample ahead because of resampler priming. The test therefore issues the note-off one sample
  earlier so that both land on the same block. In normal use the block quantisation is the same as Dexed's.
- **Sample rates.** MSFA runs at a fixed 48 kHz inside VICE SYNTH X: MSFA uses process-wide tables, and several
  plug-in instances may run at different rates. At other host rates the output is cubic-resampled, so it is not
  sample-identical to Dexed running natively at that rate. The test covers 48 kHz.
- **Listening.** No listening comparison was possible in this environment. The owner can confirm with the same patch in
  Dexed (Modern engine) and VICE SYNTH X (FM Amount 1.0, filters off, no effects).

## Reproduce
```
VSX_TEST_ONLY="FM Dexed equivalence" ViceSynthXTests                       # in-process, part of the suite
VSX_DEXED_DIR=/tmp/fmcmp VSX_TEST_ONLY="FM Dexed equivalence" ViceSynthXTests
tools/dexed_reference/build_and_compare.sh /tmp/fmcmp                      # clones Dexed 1ed5a4d, builds, compares
```

## Bug found by this comparison
`FmRenderer::noteOn (patch, …)` ignored the patch it was given once `prepare()` had run, and played the INIT voice.
The synth was not affected, because the oscillator always calls `setPatch()` first. Direct callers were affected.
It now plays the voice it is given, copied into the renderer, which is safe for temporaries and allocation-free.

## Two more integration defects found while building this test
- **Uninitialised feedback history (found with valgrind).** MSFA's `Dx7Note` never initialises its
  operator-feedback buffer (`fb_buf_`). The first block of a feedback operator in a new voice therefore depended on
  leftover memory. The symptom was a test that passed alone and failed after other tests (alg11: −38.9 dB instead of
  bit-exact). Dexed allocates its voices the same way, so it shares the latent issue.
  - Fix: the vendored files stay unmodified. `FmRenderer` constructs its MSFA state in zero-filled storage, and the
    reference does the same.
  - valgrind memcheck on the equivalence test now reports 0 errors.
- **Portamento tables never initialised.** `Dx7Note::compute` reads `Porta::rates[0]` even with portamento off. Dexed
  initialises the table at start-up (`Porta::init_sr`); VICE SYNTH X did not, so those rates were zero. It now calls
  `Porta::init_sr (48000)` with the other MSFA tables.
