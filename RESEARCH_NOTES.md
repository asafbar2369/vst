# Research notes — original AMS RMX16

## What is verified

### Hardware and signal path

- The RMX16 was introduced in 1982 and is a mono-in / stereo-out digital reverberator.
- The original audio path is a pseudo-16-bit system. The technical description specifies a 12-bit A/D conversion plus a 2-bit gain-range/exponent value, converted to a linear 16-bit word. The input is scaled by x1, x2, x4 or x8 to improve use of converter range and avoid noise-breathing behavior.
- The arithmetic section uses 28-bit internal words in 22 MHz arithmetic processors. The original manual's theory-of-operation section also describes 128 KB of memory used for delay and scratch storage.
- The original front-end sampling rate is reported as 40 kHz in the Sound On Sound technical review. This is distinct from the later RMX16 500-series unit's modern 24-bit/48 kHz converters and 32-bit DSP.
- Published original-unit performance: 20 Hz–18 kHz (+0/-3 dB), 90 dB dynamic range, typically 86 dB S/N, and less than 0.03% distortion at 1 kHz/full output.
- The system was designed and tuned by ear. The nine original programs are held in ROM/EPROM; optional barcode/remote workflows provided additional programs.

### Original nine programs and documented behavior

| # | Program | Manual-described behavior | Controls / constraints |
|---|---|---|---|
| 1 | Ambience | General-purpose neutral reverb between hall and plate, with deliberately minimized spatial/dimensional cues. Special near-freeze behavior at 9.9 with both filters at zero. | Up to 200 ms pre-delay; high- and low-frequency decay profiles. |
| 2 | Room A1 | Live-room character with deliberate coloration/standing-wave impression. Recommended short decays around 1–2 seconds and substantial HF damping. | Up to 300 ms pre-delay; HF control only; no LF equalization. |
| 3 | Hall C1 | Strong initial reflections define the hall size and give presence; useful on vocals. | High- and low-frequency decay profile controls. |
| 4 | Plate A1 | High diffusion; smooths percussive sounds; medium coloration and even decay. | Up to 300 ms pre-delay; high- and low-frequency decay-time controls; up to 5 seconds commonly useful. |
| 5 | Hall B3 | Similar to Hall C1, but with less energetic initial reflections. | Up to 200 ms pre-delay in the Issue 3 manual; high- and low-frequency controls. |
| 6 | Chorus | Five voices, spread across stereo, separated by randomized delays. Delay spread up to 50 ms (250 ms across five voices); sub-6 ms settings create phasing/flanging/tunneling. | Pre-delay changes voice separation; nudge steps 1 ms; decay controls randomization/pitch-change depth; HF filtering available. |
| 7 | Echo | Two independent programmable outputs from a single input. Decay controls feedback; pre-delay controls delay time. | A/B output modes; the AMS 500 Series Issue 2 Appendix A documents Echo A up to 1.2 s and Echo B up to 400 ms. HF/LF filtering in feedback path. |
| 8 | Nonlin 2 | Decays slightly for a controlled interval, then dies away quickly; particularly useful on drums and loudness enhancement. Stereo output. | Up to 300 ms pre-delay; HF filtering; decay scale is not calibrated. |
| 9 | Reverse 1 | Reverse-style build-up followed by a sudden stop; left output is more dispersed, right output more discrete. | Up to 100 ms pre-delay; decay scale is not calibrated. |

### Filter behavior

The original manual specifies high-frequency decay half-time values:

| HF setting | Half-decay frequency |
|---:|---:|
| 0 | 18 kHz (flat) |
| -1 | 15 kHz |
| -2 | 10 kHz |
| -3 | 8 kHz |
| -4 | 6.5 kHz |
| -5 | 5 kHz |
| -6 | 3.5 kHz |
| -7 | 2 kHz |
| -8 | 1.5 kHz |
| -9 | 1 kHz |

The LF decay control acts around 350 Hz and varies nominally from one quarter of the indicated decay time at -9 to four times at +9. The manual says to set the final values by ear because material changes the perceived result.

### UI operation

- Nudge changes programs, pre-delay (10 ms increments), decay (0.1 s increments) and filters (single-digit increments).
- Chorus is the exception for Nudge timing: its delay increments are 1 ms.
- The original display uses 11 seven-segment numeric characters plus 8 alphanumeric 17-segment characters.
- The original hardware faceplate has no modern wet/dry control. The plug-in adds a hidden/host-facing Wet/Dry parameter for DAW insert/send workflows; default is 100% wet.

## What is not publicly available

- The original EPROM contents/source code, per-program coefficient tables, complete delay tap list, exact reflection timing/weight arrays, and arithmetic instruction schedule.
- A publicly downloadable complete set of factory IR captures that can be redistributed freely.
- A direct measurement set from an original RMX16 in this project.

The UAD/AMS licensed emulation was developed with original designer Mark Crabtree and original schematics/notes; it is described by its makers as an instruction-level/bit-for-bit port. This independent project cannot claim that level of authenticity without the original code or measurement access.

## Reconstruction strategy in this source project

- Keep the original nine-program naming/order and the pictured front panel.
- Use an independently written 8-line feedback delay network with normalized Hadamard feedback, a program-specific set of delay lengths, and separate early-reflection tap patterns.
- Differentiate Ambience, Room A1, Hall C1, Plate A1 and Hall B3 by their early-reflection levels, delay lengths, diffusion and damping profiles.
- Model Chorus as five randomized/modulated delay voices spread in stereo.
- Model Echo as two regenerative delay paths with separately stored A/B pre-delay, feedback/decay and filter parameters.
- Model Nonlin 2 with a held/short-release gate envelope.
- Model Reverse 1 with a rising reverb envelope and abrupt stop.
- Approximate the original input coding with a 12-bit quantizer and four gain ranges, then 16-bit words; approximate 28-bit internal rounding; approximate 18 kHz band-limiting and 16-bit output quantization.

These are deliberately labeled approximations. The project does not yet reproduce a true 40 kHz internal resampling clock, exact converter transfer curves, or proprietary algorithm coefficients.

## Sources consulted

1. AMS/Neve RMX16 User Manual (official-hosted PDF): https://media.uaudio.com/support/manuals/dd/AMS%20RMX16%20Manual.pdf
2. Original RMX16 manual, Issue 3 (includes theory of operation and electrical specs): https://www.barryrudolph.com/recall/manuals/ams_neve_rmx_16.pdf
3. AMS Neve official RMX16 500-series product page and manual links: https://www.ams-neve.com/outboard/500-series-range/ams-rmx16/
4. Mixonline review with original hardware architecture, EPROM, memory, and delay-tap discussion: https://www.mixonline.com/technology/reviews/review-ams-rmx16-500-series-digital-reverb-module
5. Sound On Sound RMX16 review, including ADC architecture, 40 kHz original sampling, 28-bit arithmetic and licensed UAD emulation background: https://www.soundonsound.com/reviews/ams-rmx16
6. Universal Audio page for AMS RMX16 Expanded, describing Mark Crabtree's work from original schematics/notes: https://www.uaudio.com/products/rmx-16-expanded
7. Tomkin Mastering RMX16 IR package page (commercially offered IRs; not bundled with this project): https://www.tomkinmastering.com/content.php?c=ams-rmx16-ir&l=e
8. JUCE official CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
9. JUCE official releases: https://github.com/juce-framework/JUCE/releases


## Additional primary source: AMS RMX16 500 Series User Manual, Issue 2

Official PDF: https://www.ams-neve.com/wp-content/uploads/2021/08/rmx-16-500-series-user-manual-issue-2.pdf

This is a valuable manufacturer-published source. It must be used carefully: it documents the 500 Series hardware and its control behavior, while its Appendix A also describes the original nine programs. The 500 Series is not the original converter/DSP hardware.

### Confirmed from the official 500 Series manual

- The 500 Series runs 24-bit / 48 kHz conversion and 32-bit DSP; the manual says it uses the very same algorithms to recreate the original sound. This is not evidence that the original hardware itself ran at 48 kHz or used 32-bit DSP.
- The manual explicitly lists the original nine in this order: Ambience, Room A1, Hall C1, Plate A1, Hall B3, Chorus, Echo, Nonlin 2, Reverse 1. The 500 Series adds Reverse 2, Freeze, Room A0, Room B1, Hall A1, Plate B1, Delay, Image P1 and Nonlin 1 (18 programs total).
- Hall B3 maximum pre-delay is 200 ms (Appendix A, printed page 20). This corroborates the Issue 3 original manual and the corrected source clamp.
- Echo is two independent outputs: Echo A maximum 1.2 s; Echo B maximum 400 ms. Decay controls regeneration/feedback, and pre-delay controls the delay time.
- Chorus is a five-voice stereo program with randomized delays. Pre-delay controls voice separation up to 50 ms; Nudge steps are 1 ms; decay controls randomization/pitch-change depth; HF filtering is available.
- Nonlin 2 holds/decays only slightly for a period, then dies away rapidly; it is stereo, has up to 300 ms pre-delay, and HF filtering.
- Reverse 1 has a more dispersed left output and more discrete right output; it builds then stops abruptly; maximum pre-delay is 100 ms.
- Ambience supports a near-infinite/freeze-like setting at 9.9 with both filters at zero. The manual warns of DC-offset accumulation and eventual overflow after several/tens of minutes; returning decay briefly to zero clears it.
- Decay-filter polarity is semantically meaningful: positive values increase decay time in the selected frequency band, negative values decrease it. These are decay-profile controls, not ordinary static EQ knobs.
- The 500 Series introduces controls/features not present on the original front panel, including Mix and non-volatile user presets (up to 100). These should not be represented as original hardware controls.
- The manual's modern 500 Series specifications include >+22 dBu headroom, 112 dB dynamic range, typically ±0.25 dB from 20 Hz to 18 kHz, and typically 0.002% THD+N at 1 kHz. These figures must not be attributed to the original RMX16.

### Reconciliation and implementation consequences

1. Keep the plugin's target as the original nine-program RMX16, not the 18-program 500 Series. Do not silently add the nine extra programs to the main program selector.
2. Preserve original hardware distinctions: mono input, stereo outputs, no original wet/dry Mix control, and original converter/clock behavior as documented separately. A host-facing wet/dry parameter may remain as a DAW convenience but must be labeled as a plugin addition.
3. Keep Hall B3 at 200 ms maximum.
4. Keep Echo A/B independent with 1200 ms / 400 ms maximum delay values, respectively; validate the implementation's parameter routing and stereo outputs against this.
5. Chorus must be evaluated as five independently delayed/panned voices with randomized timing, not as a conventional chorus LFO preset.
6. Treat Ambience freeze behavior as a special operating region with explicit stability/overflow handling, not merely as an arbitrary long decay value.
7. Do not infer internal DSP topology from the 500 Series specifications. The manufacturer states algorithm continuity but does not publish source code, coefficient tables, or complete delay structures in this manual.

### Source citations by manual page

- Introduction and algorithm tuning philosophy: printed pp. 5–6.
- 500 Series 24-bit/48 kHz and 32-bit DSP; mono-in/stereo-out architecture: printed p. 6.
- Control operation, input/output level, Mix behavior: printed pp. 9–13.
- 500 Series specifications: printed p. 17.
- Program index and Ambience: printed p. 18.
- Room A1, Hall C1, Plate A1, Hall B3: printed p. 19.
- Chorus, Echo, Nonlin 2, Reverse 1: printed pp. 19–20.
- Extra programs and their distinctions: printed pp. 20–23.
