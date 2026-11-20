#!/usr/bin/env python3
"""Usage: make_reports.py <outdir> <log>...
Builds the measured tables of the QA documents from test logs (MEASURE|area|item|value|unit lines)."""
import sys, re, collections
outdir = sys.argv[1]
logs = sys.argv[2:]
rows = []
for path in logs:
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('MEASURE|'):
            parts = line.rstrip('\n').split('|')
            if len(parts) >= 5:
                rows.append((parts[1], parts[2], parts[3], '|'.join(parts[4:])))
by = collections.defaultdict(list)
for a, item, v, u in rows: by[a].append((item, v, u))
def table(area, title, filt=None):
    out = [f"### {title}", "", "| Measurement | Value | Unit |", "|---|---:|---|"]
    for item, v, u in by.get(area, []):
        if filt and not filt(item): continue
        out.append(f"| {item} | {v} | {u} |")
    return "\n".join(out) + "\n"
sections = {
 'filters': table('filter', 'Filters (internal rate 96 kHz, cutoff 1 kHz unless noted)'),
 'oscillators': table('oscillator', 'Oscillators (48 kHz)'),
 'fx': table('fx', 'Effects (C4 pluck through each effect, 48 kHz)'),
 'cpu': table('cpu', 'Processor realtime factor and peak (Ocean Drive Poly, 6-note chord)'),
 'latency': table('latency', 'Reported latency'),
 'voices': table('voices', 'Voice allocation'),
 'state': table('state', 'State') + table('automation', 'Automation') + table('longrun', 'Long run'),
 'library': table('library', 'Library summary'),
 'presets': table('preset', 'Per-preset level (as played) and peak'),
 'matching': table('matching', 'Sound matching benchmark'),
}
for k, v in sections.items():
    open(f"{outdir}/section_{k}.md", "w").write(v)
print("measurements:", len(rows), "areas:", ", ".join(sorted(by)))
