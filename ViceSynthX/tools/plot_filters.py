"""Plots filter measurements exported by the test runner (VSX_FILTER_CSV=<dir>, VSX_TEST_ONLY="Filter quality").
usage: python3 tools/plot_filters.py <csv_dir> <out_dir> [label]"""
import sys, csv, pathlib
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
label = sys.argv[3] if len(sys.argv) > 3 else ""
out.mkdir(parents=True, exist_ok=True)
plt.style.use("dark_background")
files = sorted(p for p in src.glob("filter_*.csv") if p.name != "filter_thd.csv")
fig, axes = plt.subplots(3, 3, figsize=(15, 11), sharex=True, sharey=True)
for ax, f in zip(axes.flat, files):
    rows = list(csv.reader(open(f)))
    head, data = rows[0], [[float(v) for v in r] for r in rows[1:]]
    hz = [r[0] for r in data]
    for j, name in enumerate(head[1:], start=1):
        ax.semilogx(hz, [r[j] for r in data], label=name.replace("res", "res "))
    ax.set_title(f.stem.replace("filter_", "").replace("_", " ").title() + " LP24, fc 1 kHz")
    ax.set_ylim(-60, 24); ax.set_xlim(20, 20000); ax.grid(alpha=0.25, which="both")
axes[0][0].legend(fontsize=8)
for ax in axes[-1]: ax.set_xlabel("Hz")
for ax in axes[:, 0]: ax.set_ylabel("dB")
fig.suptitle(f"Small-signal magnitude response by resonance {label}".strip())
fig.tight_layout(); fig.savefig(out / "filter_responses.png", dpi=100); plt.close(fig)

thd = list(csv.reader(open(src / "filter_thd.csv")))
fig, ax = plt.subplots(figsize=(11, 5.5))
drives = [float(h[1:]) for h in thd[0][1:]]
for r in thd[1:]:
    ax.plot(drives, [float(v) for v in r[1:]], marker="o", label=r[0])
ax.set_xlabel("Drive"); ax.set_ylabel("THD (%) - 110 Hz sine, 0.5 FS, fc 8 kHz"); ax.set_yscale("log")
ax.set_title(f"Filter saturation vs drive {label}".strip()); ax.grid(alpha=0.25, which="both"); ax.legend(ncol=3, fontsize=8)
fig.tight_layout(); fig.savefig(out / "filter_thd.png", dpi=110); plt.close(fig)
print("wrote", out)
