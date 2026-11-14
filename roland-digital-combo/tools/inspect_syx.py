#!/usr/bin/env python3
"""Audit user-provided Roland D-50 / JD-800 .syx files without JUCE or ROM data.

Recognises only verified one-way DT1 format (F0 41 device model 12 address data checksum F7).
Maps memory writes into the 64 patch slots of each instrument and reports *coverage*:
writing one byte never counts as a complete patch. Does not claim to decode PCM or
handshake DAT/EOD transfers. No SysEx presets are bundled with this utility.
"""
from __future__ import annotations
from collections import defaultdict
from pathlib import Path
import argparse
import json
import sys

MODEL = {0x14: ('D-50', 448, 0x02 << 14, 18, 384),
         0x3D: ('JD-800', 384, 0x05 << 14, 16, 0)}
MAX_FILE_BYTES = 8 * 1024 * 1024

def messages(raw: bytes):
    """Yield candidate F0..F7 frames, skipping garbage and truncated fragments."""
    start = None
    for i, b in enumerate(raw):
        if b == 0xF0:
            start = i
        elif b == 0xF7 and start is not None:
            yield raw[start:i + 1]
            start = None

def decode_name(model: str, data: dict[int, int], start: int, length: int) -> str | None:
    if not all(start + i in data for i in range(length)):
        return None
    if model == 'JD-800':
        return ''.join(chr(data[start + i]) if 32 <= data[start + i] <= 126 else '?' for i in range(length)).strip()
    # D-50 encoded alphabet: ' ', A-Z, a-z, 1-9, 0, -
    alphabet = ' ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-'
    return ''.join(alphabet[data[start + i]] if data[start + i] < len(alphabet) else '?' for i in range(length)).strip()

def inspect(data: bytes) -> dict:
    slots: dict[str, dict[int, dict[int, int]]] = {'D-50': defaultdict(dict), 'JD-800': defaultdict(dict)}
    counters = {'frames': 0, 'dt1_valid': 0, 'bad_checksum': 0, 'malformed': 0,
                'unsupported': 0, 'not_roland': 0, 'outside_patch_memory': 0}
    for frame in messages(data):
        counters['frames'] += 1
        if len(frame) < 3 or frame[1] != 0x41:
            counters['not_roland'] += 1
            continue
        if len(frame) < 5:
            counters['malformed'] += 1
            continue
        model = MODEL.get(frame[3])
        if model is None:
            counters['unsupported'] += 1
            continue
        if frame[4] != 0x12:
            counters['unsupported'] += 1  # DAT and handshake aren't treated as valid DT1.
            continue
        if (not 1 <= len(frame) - 10 <= 256) or any(b >= 128 for b in frame[1:-1]):
            counters['malformed'] += 1
            continue
        address = frame[5:8]
        payload = frame[8:-2]
        if ((sum(address) + sum(payload) + frame[-2]) & 127) != 0:
            counters['bad_checksum'] += 1
            continue
        counters['dt1_valid'] += 1
        model_name, stride, base, _, _ = model
        addr = (address[0] << 14) | (address[1] << 7) | address[2]
        for delta, value in enumerate(payload):
            offset = addr + delta - base
            if 0 <= offset < 64 * stride:
                slot, pos = divmod(offset, stride)
                slots[model_name][slot][pos] = value
            else:
                counters['outside_patch_memory'] += 1
    inventory = {}
    for _, (model_name, stride, _, name_len, name_start) in MODEL.items():
        entries = []
        for slot in sorted(slots[model_name]):
            vals = slots[model_name][slot]
            entries.append({'slot': slot + 1, 'display_slot': f'{slot // 8 + 1}-{slot % 8 + 1}',
                            'covered_bytes': len(vals), 'total_bytes': stride,
                            'complete': len(vals) == stride,
                            'name': decode_name(model_name, vals, name_start, name_len)})
        inventory[model_name] = {'slots': entries, 'complete_count': sum(e['complete'] for e in entries),
                                 'partial_count': sum(not e['complete'] for e in entries)}
    return {'stats': counters, **inventory,
            'notes': ['Only valid DT1 frames are counted.',
                      'A complete byte map is not proof that an original Roland patch is authentic.',
                      'Only patch-memory locations are reported; temporary-area DT1 updates are not patch-bank slots.']}

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('syx', type=Path, nargs='+', help='One or more user-supplied .syx files')
    parser.add_argument('--json', action='store_true', help='JSON report')
    args = parser.parse_args(argv)
    out = []
    for path in args.syx:
        if not path.is_file() or path.stat().st_size > MAX_FILE_BYTES:
            out.append({'file': str(path), 'error': 'missing or larger than 8 MiB'})
            continue
        out.append({'file': str(path), **inspect(path.read_bytes())})
    if args.json:
        print(json.dumps(out, ensure_ascii=False, indent=2))
    else:
        for record in out:
            print(record['file'], record.get('error', ''))
            if 'stats' in record:
                print('  DT1 valid:', record['stats']['dt1_valid'], 'bad checksum:', record['stats']['bad_checksum'])
                for name in ('D-50', 'JD-800'):
                    print(' ', name, 'complete:', record[name]['complete_count'], 'partial:', record[name]['partial_count'])
                    for slot in record[name]['slots']:
                        print('    ', slot['display_slot'], slot['covered_bytes'], '/', slot['total_bytes'],
                              'name:', slot['name'] or '?')
    return int(any('error' in r for r in out))

if __name__ == '__main__':
    sys.exit(main())
