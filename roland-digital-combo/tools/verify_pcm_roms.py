#!/usr/bin/env python3
"""Identify *user supplied* D-50 and JD-800 PCM ROM chip dumps, without decoding them.

No copyrighted bytes are supplied, downloaded or redistributed. SHA/CRC identifiers
come from public MAME ROM metadata. A verified dump is NOT yet a playable waveform:
proper proprietary PCM decoding and the multisample tables are separate work.

Usage: python tools/verify_pcm_roms.py /path/to/rom-folder [another.zip] [--json]
"""
from __future__ import annotations
import argparse
from dataclasses import asdict, dataclass
from pathlib import Path
import hashlib
import json
import sys
import zipfile
import zlib

MAX_FILE_BYTES = 8 * 1024 * 1024

@dataclass(frozen=True)
class Chip:
    instrument: str
    role: str
    size: int
    crc32: str
    sha1: str = ""

# MAME machine data for d50/d50o and jd800. 'firmware' is explicitly excluded.
CHIPS = (
    Chip('D-50', 'PCM combined A+B', 524288, 'e2aed2d9', 'e9f5b38b9b5fce04beb4cf871401e821a42edacb'),
    Chip('D-50', 'PCM A', 262144, '1461c0fb', '55257e70bcf003439b76f96b7ae7e45a3cf24276'),
    Chip('D-50', 'PCM B', 262144, 'e50599bf', '487c62f2ef7baccf2421059a940fa707e27aefb2'),
    Chip('JD-800', 'PCM A / 2A6', 1048576, 'd743cec8'),
    Chip('JD-800', 'PCM B / 2A7', 1048576, '06098658'),
    Chip('JD-800', 'PCM C / 2A8', 1048576, '5c83e539'),
)

def classify(data: bytes, chips: tuple[Chip, ...] = CHIPS) -> dict:
    crc = f'{zlib.crc32(data) & 0xffffffff:08x}'
    sha = hashlib.sha1(data).hexdigest()
    matches = [c for c in chips if c.size == len(data) and c.crc32 == crc and (not c.sha1 or c.sha1 == sha)]
    return {'bytes': len(data), 'crc32': crc, 'sha1': sha,
            'instrument': matches[0].instrument if matches else None,
            'role': matches[0].role if matches else None,
            'verified': bool(matches)}

def iter_entries(paths: list[Path]):
    for path in paths:
        if path.is_dir():
            yield from iter_entries(sorted(p for p in path.rglob('*') if p.is_file()))
        elif path.suffix.lower() == '.zip':
            with zipfile.ZipFile(path) as z:
                for member in z.infolist():
                    if member.is_dir():
                        continue
                    if member.file_size > MAX_FILE_BYTES:
                        yield f'{path.name}:{member.filename}', None, 'skipped (larger than 8 MiB)'
                        continue
                    try:
                        with z.open(member) as f:
                            data = f.read(MAX_FILE_BYTES + 1)
                    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
                        yield f'{path.name}:{member.filename}', None, f'unreadable ({exc})'
                        continue
                    yield f'{path.name}:{member.filename}', data, None
        elif path.is_file():
            if path.stat().st_size > MAX_FILE_BYTES:
                yield str(path), None, 'skipped (larger than 8 MiB)'
                continue
            try:
                yield str(path), path.read_bytes(), None
            except OSError as exc:
                yield str(path), None, f'unreadable ({exc})'
        else:
            yield str(path), None, 'not found'

def audit(paths: list[Path]) -> dict:
    entries = []
    found = {'D-50': set(), 'JD-800': set()}
    for label, data, error in iter_entries(paths):
        row = {'file': label, **(classify(data) if data is not None else {'verified': False})}
        if error:
            row['error'] = error
        if row.get('verified'):
            found[row['instrument']].add(row['role'])
        entries.append(row)
    return {
        'files': entries,
        'D-50': {'pcm_dump_complete': 'PCM combined A+B' in found['D-50'] or
                 {'PCM A', 'PCM B'} <= found['D-50'], 'identified_roles': sorted(found['D-50'])},
        'JD-800': {'pcm_dump_complete': {'PCM A / 2A6', 'PCM B / 2A7', 'PCM C / 2A8'} <= found['JD-800'],
                   'identified_roles': sorted(found['JD-800'])},
        'note': 'Recognition only. No hardware PCM decoding, multisample mapping or playback is implemented by this tool.',
    }

def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('paths', type=Path, nargs='+', help='Your own ROM files, directories, or MAME-style ZIPs')
    p.add_argument('--json', action='store_true', help='Output machine-readable JSON')
    args = p.parse_args(argv)
    result = audit(args.paths)
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        for f in result['files']:
            print(('VERIFIED' if f['verified'] else 'UNKNOWN'), f['file'],
                  f.get('instrument', ''), f.get('role', ''), f.get('error', ''))
        for name in ('D-50', 'JD-800'):
            print(name, 'COMPLETE' if result[name]['pcm_dump_complete'] else 'INCOMPLETE',
                  ', '.join(result[name]['identified_roles']))
        print(result['note'])
    return 0 if any(f['verified'] for f in result['files']) else 1

if __name__ == '__main__':
    sys.exit(main())
