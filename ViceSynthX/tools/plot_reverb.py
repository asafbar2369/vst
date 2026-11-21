"""Plots reverb impulse responses exported by the test runner (VSX_IR_DIR).
usage: python3 tools/plot_reverb.py <ir_dir> <out_dir>
Writes reverb_edc.png (Schroeder energy decay), reverb_spectrum.png (1/3-octave magnitude of the response) and
reverb_ir_<name>.png (waveform of the first 400 ms)."""
import sys, wave, pathlib
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def load(p):
    with wave.open(str(p)) as w:
        n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    assert sw == 3
    b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
    v = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
    v = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float64) / (1 << 23)
    v = v.reshape(-1, ch)
    return v.mean(axis=1), sr

ir_dir, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
order = ["legacy", "hall", "plate", "room", "chamber", "ambient", "nonlinear", "gated", "reverse"]
irs = {k: load(ir_dir / f"ir_{k}.wav") for k in order if (ir_dir / f"ir_{k}.wav").exists()}
plt.style.use("dark_background")

fig, ax = plt.subplots(figsize=(11, 6))
for k, (x, sr) in irs.items():
    edc = np.cumsum((x ** 2)[::-1])[::-1]
    edc_db = 10 * np.log10(np.maximum(edc / edc[0], 1e-12))
    t = np.arange(len(x)) / sr
    ax.plot(t, edc_db, label=k, linewidth=2.2 if k == "legacy" else 1.3, linestyle="--" if k == "legacy" else "-")
ax.set_ylim(-80, 2); ax.set_xlim(0, 4); ax.set_xlabel("time (s)"); ax.set_ylabel("energy decay (dB)")
ax.set_title("Schroeder energy decay at the default settings (Decay 3 s, Size 60 %, Damping 40 %)")
ax.grid(alpha=0.25); ax.legend(ncol=3)
fig.tight_layout(); fig.savefig(out / "reverb_edc.png", dpi=110); plt.close(fig)

fig, ax = plt.subplots(figsize=(11, 6))
for k, (x, sr) in irs.items():
    spec = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / sr)
    centres = 1000 * 2 ** (np.arange(-17, 14) / 3)
    lv = []
    for c in centres:
        m = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        lv.append(10 * np.log10(max(spec[m].mean() if m.any() else 0.0, 1e-20)))
    lv = np.array(lv); lv -= lv[np.argmin(abs(centres - 1000))]
    ax.semilogx(centres, lv, label=k, linewidth=2.2 if k == "legacy" else 1.3, linestyle="--" if k == "legacy" else "-")
ax.set_xlim(20, 20000); ax.set_ylim(-40, 10); ax.set_xlabel("frequency (Hz)"); ax.set_ylabel("power density re 1 kHz (dB, 1/3-octave averages; white = flat)")
ax.set_title("Response spectrum (whole impulse response)"); ax.grid(alpha=0.25, which="both"); ax.legend(ncol=3)
fig.tight_layout(); fig.savefig(out / "reverb_spectrum.png", dpi=110); plt.close(fig)

fig, axes = plt.subplots(len(irs), 1, figsize=(11, 1.4 * len(irs)), sharex=True)
for ax, (k, (x, sr)) in zip(axes, irs.items()):
    n = int(0.4 * sr)
    ax.plot(np.arange(n) / sr * 1000, x[:n] / max(1e-9, np.abs(x[:n]).max()), linewidth=0.5)
    ax.set_ylabel(k, rotation=0, ha="right", va="center"); ax.set_yticks([])
axes[-1].set_xlabel("time (ms)")
fig.suptitle("First 400 ms of each impulse response (normalised)")
fig.tight_layout(); fig.savefig(out / "reverb_ir_waveforms.png", dpi=110); plt.close(fig)
print("wrote", out)
