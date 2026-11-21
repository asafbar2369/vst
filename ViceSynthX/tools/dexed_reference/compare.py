#!/usr/bin/env python3
# Compare VICE SYNTH X FmRenderer exports against the Dexed reference renderer.
# usage: compare.py <export_dir> <dexed_ref_binary> <out.csv>
import sys, os, subprocess, glob
import numpy as np

d, ref_bin, out_csv = sys.argv[1], sys.argv[2], sys.argv[3]
SR = 48000

def dc_block(x):
    from scipy.signal import lfilter
    r = 1.0 - 126.0 / SR
    return lfilter([1.0, -1.0], [1.0, -r], x.astype(np.float64)).astype(np.float32)

def centroid(x):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    return float((X * f).sum() / max(X.sum(), 1e-20))

rows = []
for b in sorted(glob.glob(os.path.join(d, "*.bin"))):
    cid = os.path.basename(b)[:-4]
    vsx = np.fromfile(os.path.join(d, cid + ".vsx.f32"), dtype=np.float32)
    n = len(vsx)
    reff = os.path.join(d, cid + ".dexed.f32")
    subprocess.run([ref_bin, b, os.path.join(d, cid + ".txt"), reff, str(n)], check=True)
    ref = np.fromfile(reff, dtype=np.float32)
    # Dexed's DC blocker belongs to Dexed's output stage: apply the same filter to both
    vsx = dc_block(vsx)
    best = None
    for lag in range(0, 6):
        a = ref[: n - lag]; v = vsx[lag:]
        den = float(np.dot(a, a))
        if den <= 0: continue
        g = float(np.dot(a, v) / den)
        e = v - g * a
        res = 10 * np.log10(max(float(np.dot(e, e)), 1e-30) / den)
        if best is None or res < best[1]: best = (lag, res, g)
    if best is None:
        print('  SILENT REFERENCE:', cid, 'vsx peak %.3f' % float(np.abs(vsx).max())); continue
    lag, res, g = best
    a = ref[: n - lag]; v = vsx[lag:]
    e = v - a  # unity-gain residual too
    res_unity = 10 * np.log10(max(float(np.dot(e, e)), 1e-30) / max(float(np.dot(a, a)), 1e-30))
    seg = slice(SR // 2, SR // 2 + 16384)
    rows.append((cid, lag, 20 * np.log10(abs(g) + 1e-30), res, res_unity,
                 20 * np.log10(np.abs(a).max() + 1e-30), 20 * np.log10(np.abs(v).max() + 1e-30),
                 centroid(a[seg]), centroid(v[seg])))

with open(out_csv, "w") as f:
    f.write("case,latency_samples,gain_db,residual_db_bestgain,residual_db_unity,peak_dexed_dbfs,peak_vsx_dbfs,centroid_dexed_hz,centroid_vsx_hz\n")
    for r in rows:
        f.write("%s,%d,%.3f,%.1f,%.1f,%.2f,%.2f,%.0f,%.0f\n" % r)
res = np.array([r[3] for r in rows])
print("cases %d  residual (best gain) median %.1f dB  worst %.1f dB (%s)" % (len(rows), np.median(res), res.max(), rows[int(res.argmax())][0]))
print("gain dB: min %.3f max %.3f; latency set: %s" % (min(r[2] for r in rows), max(r[2] for r in rows), sorted(set(r[1] for r in rows))))
for r in rows:
    if r[3] > -60: print("  above -60 dB:", r[0], "residual %.1f" % r[3], "unity %.1f" % r[4])
