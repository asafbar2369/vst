# DSP Research Notes and Bibliography

How to read this file: each algorithm lists the source the implementation follows, the principle, the implementation in
VICE SYNTH X, limitations, cost, stability notes and the verification test. **Citations are given from the literature as known
to the authors of this document; the publisher sites (dafx.de, aes.org, ccrma.stanford.edu, arxiv.org, dsprelated.com,
musicdsp.org) could not be reached from the build environment, so links were not re-checked on 2026-11-20.**

| Topic | Primary source | VICE implementation | Limitations | Cost | Verification |
|---|---|---|---|---|---|
| Band-limited oscillators (PolyBLEP / BLAMP) | V. Valimaki, A. Huovilainen, "Antialiasing oscillators in subtractive synthesis", IEEE Signal Processing Magazine 24(2), 2007; F. Esqueda, V. Valimaki, S. Bilbao, "Rounding corners with BLAMP", DAFx-16, 2016 | 2-sample polyBLEP on saw / pulse steps, BLAMP on wavefold corners; plus 1x/2x/4x oversampling of the whole voice | residual aliasing at 1x on very high notes (-27 dB measured) | O(1) per sample | TestMeasurements: C7 inharmonic energy per quality |
| Wavetable playback | mip-mapped band-limited tables (common practice; e.g. "Bandlimited wavetable synthesis", various) | 11 mip levels per frame, 2048-sample frames, linear / smooth frame interpolation | memory per table; interpolation noise at very low notes not measured | O(1) | pitch-accuracy test |
| Zero-delay-feedback / TPT filters | V. Zavalishin, *The Art of VA Filter Design*, Native Instruments, rev. 2.1.2, 2020 | all filters use TPT integrators with instantaneous feedback solved per sample | nonlinear models use approximations (tanh in stages, secant solve) | 4-stage: ~40-70 ops/sample/channel | filter slope / stability tests |
| Moog-style ladder | A. Huovilainen, "Non-linear digital implementation of the Moog ladder filter", DAFx-04, 2004; T. Stilson, J. O. Smith, "Analyzing the Moog VCF with considerations for digital implementation", ICMC 1996 | TPT ladder with per-stage tanh and resonance compensation | not fitted to a unit | as above | self-oscillation tuning (993 Hz @ 1 kHz) |
| Diode ladder | Zavalishin (above); numerically derived constants (w180, w3dB, k_osc) for equal and unequal capacitors | implicit 4x4 solve with secant conductances | cutoff calibration places resonance 3x above the cutoff value | higher than ladder | measured resonance frequency, slopes |
| Korg35 / MS-20 style | W. Pirkle, "Virtual Analog (VA) Korg35 Lowpass Filter v2.0", application note, 2013; W. Pirkle, *Designing Software Synthesizer Plug-Ins in C++*, Focal Press, 2015 | TPT Sallen-Key with clipping feedback | HP slope measured ~6 dB/oct | low | measurements |
| State-variable filter | A. Simper, "Linear trapezoidal integrated state variable filter", Cytomic technical paper, 2013; Zavalishin (above) | TPT SVF with saturating integrator and input stage | SEM model does not self-oscillate (by design) | low | measurements |
| OTA cascades | OTA behaviour as tanh of differential input (standard large-signal model); Zavalishin ch. on nonlinear models | 4 OTA stages with feedback saturation variants | algorithmic approximation, not fitted | medium | measurements |
| FM / PM / RM / sync | J. Chowning, "The synthesis of complex audio spectra by means of frequency modulation", JAES 21(7), 1973 | per-oscillator FM / PM / RM / AM / hard sync from another oscillator | no 6-operator FM engine | O(1) | render tests (finite, non-silent) |
| Granular synthesis | C. Roads, *Microsound*, MIT Press, 2001 | windowed grains (Hann / triangle / Tukey / exponential) with density, spray, pitch randomisation | no grain visualisation | grows with density | render tests |
| Spectral resynthesis | X. Serra, J. O. Smith, "Spectral modeling synthesis", CMJ 14(4), 1990 (sinusoidal modelling) | harmonic partial extraction to additive resynthesis frames | harmonic sounds only; no residual / noise model | partials x voices | render tests |
| Karplus-Strong (built-in pluck samples) | K. Karplus, A. Strong, "Digital synthesis of plucked-string and drum timbres", CMJ 7(2), 1983; D. Jaffe, J. O. Smith, "Extensions of the Karplus-Strong plucked-string algorithm", CMJ 7(2), 1983 | used offline to synthesise the original "Nylon Pluck" samples | not a real-time physical model | offline | library renders |
| Feedback delay network reverb | J.-M. Jot, A. Chaigne, "Digital delay networks for designing artificial reverberators", AES 90th Convention, 1991; J. Stautner, M. Puckette, "Designing multi-channel reverberators", CMJ 6(1), 1982 | 8-line FDN with normalised Hadamard mixing, loop gains from the RT60 setting, damping low-pass per line, input all-pass diffusion | broadband RT60 only (no per-band decay); no delay-line modulation | 8 delay lines + 8 diffusers per sample | effect tests (finite, tail level) |
| Convolution | partitioned FFT convolution (JUCE `dsp::Convolution`) | user or generated IRs, background loading | latency of the JUCE engine; IR CPU cost | FFT per block | effect tests |
| Hilbert transformer / frequency shifter | O. Niemitalo, "Hilbert transform: polyphase IIR allpass pair" (coefficients), 2001 | two 4-stage all-pass chains (~90 degree difference) | sideband rejection not measured yet | low | effect tests |
| Oversampling | polyphase IIR half-band filters (JUCE `dsp::Oversampling`) | 1x / 2x / 4x over the whole voice | 3-sample latency at 2x | moderate | latency report test |
| Pitch detection | A. de Cheveigne, H. Kawahara, "YIN, a fundamental frequency estimator for speech and music", JASA 111(4), 2002 | YIN on reference audio for matching | monophonic material only | O(N W) offline | 440 Hz sine test |
| Parameter smoothing | one-pole / linear ramps per control tick (common practice) | 16-sample control rate, sample-accurate MIDI splits | no per-sample modulation of every parameter | low | automation fuzz (pluginval, tests) |
| Denormals | flush-to-zero (`juce::ScopedNoDenormals`) plus explicit flushing of filter states | | | | long-run test |

## Research still required (not implemented)
- Physical / modal modelling (e.g. digital waveguides: J. O. Smith, *Physical Audio Signal Processing*, CCRMA, 2010).
- 6-operator FM with feedback (Chowning; DX7-family algorithm structure as inspiration only).
- Phase distortion, vector synthesis, wave sequencing.
- SIMD (SSE/AVX2) voice processing and multi-core rendering.
- Hardware-measured filter fits (see HARDWARE_MODEL_ACCURACY.md).
