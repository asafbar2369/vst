#!/usr/bin/env bash
# Independent check of VICE SYNTH X's FM output against Dexed's "Modern" engine, built from a separate Dexed checkout.
# 1. Export the VICE SYNTH X renders:   VSX_DEXED_DIR=<dir> VSX_TEST_ONLY="FM Dexed equivalence" ViceSynthXTests
# 2. Run this script:                    tools/dexed_reference/build_and_compare.sh <dir>
# The reference is compiled from Dexed's own source/msfa (Apache 2.0) at the pinned commit. Dexed's tuning files need
# the GPLv3 tuning-library submodule, so standard tuning comes from VICE SYNTH X's tuning.* (same table: base
# 50857777, step (1 << 24) / 12). Nothing from this script is shipped in the plug-in.
set -euo pipefail
EXPORT_DIR="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
WORK="$(mktemp -d)"
git clone -q --filter=blob:none https://github.com/asb2m10/dexed.git "$WORK/dexed"
git -C "$WORK/dexed" checkout -q 1ed5a4d03157c35db098dfa367372c8b3c0ab82e
mkdir -p "$WORK/ref/msfa" "$WORK/ref/debugger"
cp "$WORK"/dexed/source/msfa/*.cc "$WORK"/dexed/source/msfa/*.h "$WORK"/dexed/source/msfa/*.cpp "$WORK/ref/msfa/"
rm "$WORK/ref/msfa/tuning.h" "$WORK/ref/msfa/tuning.cc"
cp "$REPO"/Source/ThirdParty/msfa/{tuning.h,tuning.cc,libMTSClient.h} "$WORK/ref/msfa/"
cp "$REPO/Source/ThirdParty/debugger/Logger.h" "$WORK/ref/debugger/"
cp "$HERE/dexed_ref.cc" "$WORK/ref/"
( cd "$WORK/ref" && g++ -O2 -std=c++17 -I. -Imsfa dexed_ref.cc msfa/*.cc msfa/*.cpp -o dexed_ref )
python3 "$HERE/compare.py" "$EXPORT_DIR" "$WORK/ref/dexed_ref" "$EXPORT_DIR/results.csv"
echo "results: $EXPORT_DIR/results.csv"
