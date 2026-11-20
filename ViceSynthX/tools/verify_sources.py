#!/usr/bin/env python3
"""Fail early with actionable diagnostics for an incomplete VSX source checkout."""
import argparse
from pathlib import Path
import re
import sys

SOURCE_RE = re.compile(r"(?m)^\s*((?:Source|Tests)/[^\s)]+\.cpp)\s*\)?\s*$")


def missing_sources(cmake_text: str, exists, include_tests: bool = True) -> list[str]:
    files = set(SOURCE_RE.findall(cmake_text))
    if not files:
        raise ValueError("No C++ sources found in CMakeLists.txt")
    return sorted(p for p in files
                  if (include_tests or not p.startswith("Tests/")) and not exists(p))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--no-tests", action="store_true", help="Only check plugin sources")
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        missing = missing_sources((root / "CMakeLists.txt").read_text(encoding="utf-8"),
                                  lambda p: (root / p).is_file(), not args.no_tests)
    except (OSError, ValueError) as exc:
        print(f"Source audit failed: {exc}", file=sys.stderr)
        return 2
    if missing:
        print(f"Source audit FAILED: {len(missing)} missing compilation unit(s):", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        return 1
    print("Source audit passed (file presence only, not compilation).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
