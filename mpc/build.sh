#!/usr/bin/env bash
# Build, test and package the MPC OS (VST2, Linux ARM) versions of the four Golden Era plug-ins.
#   MPC_VST=/path/to/mpc-vst-plugins mpc/build.sh <version> [out-dir]
# Uses the mpc-vst-plugins tools (pinned in .github/workflows/mpc.yml): build_port.sh builds each .so in Docker with
# arm32v7/gcc:11-bullseye (glibc 2.31, Gen1 MPC + Force) and arm64v8/gcc:12-bookworm (Gen2), so Docker with QEMU is needed.
# Output: <out-dir>/<Name>-<version>-mpc-armv7.zip and -mpc-aarch64.zip per plug-in, plus skin previews.
set -euo pipefail
VERSION="${1:?usage: mpc/build.sh <version> [out-dir]}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mkdir -p "${2:-$ROOT/mpc/dist}" && cd "${2:-$ROOT/mpc/dist}" && pwd)"
: "${MPC_VST:?set MPC_VST to a checkout of sd88me/mpc-vst-plugins}"
cd "$ROOT"

declare -A ABOUT=(
  [EraQ]="8-band EQ with Stereo/Mid/Side bands and an analog Color stage, tuned for 90s boom bap, New Jack Swing and R&B."
  [EraC]="Compressor with Clean, Punch, Opto and Slam styles, auto release and parallel mix."
  [EraS]="Tape, Tube and Console saturation plus SP 26k, Sixty 40k and Vari-Rate sampler grit and vinyl dust."
  [EraL]="Look-ahead limiter with Clean, Punchy, Warm and Loud '95 styles."
)

for P in EraQ EraC EraS EraL; do
  echo "::group::$P"
  NAME=$(python3 -c "import json;print(json.load(open('mpc/$P/vst.json'))['name'])")
  SO=$(python3 -c "import json;print(json.load(open('mpc/$P/vst.json'))['so'])")
  SKIN="mpc/$P/build/skin/8cee - VST - $NAME"

  "$MPC_VST/tools/build_port.sh" "mpc/$P/vst.json" all
  "$MPC_VST/tools/test_port.sh" "mpc/$P/vst.json"

  # audio test: every preset through the engine (x86), NaN / silence / state round trip
  g++ -O2 -std=gnu++11 -ffast-math -Impc/common -I"$MPC_VST/wrapper" -I"mpc/$P/build" \
      mpc/tests/engine_test.cpp "mpc/$P/$(echo "$P" | tr A-Z a-z)_engine.cpp" -o "mpc/$P/build/engine_test" -lm
  mkdir -p "mpc/$P/build/et" && "mpc/$P/build/engine_test" "mpc/$P/build/et"

  python3 "$MPC_VST/tools/skin_check.py" "$SKIN"
  mkdir -p "$OUT/previews"
  python3 "$MPC_VST/tools/studio.py" preview "$SKIN/Plugin Skins" -o "$OUT/previews/${P}_%d.png"

  for so in "mpc/$P/build/$SO" "mpc/$P/build/aarch64/$SO"; do
    [ -f "$so" ] || continue
    python3 "$MPC_VST/tools/release.py" --so "$so" --skin "$SKIN" --entry "mpc/$P/build/pluginlist-entry.xml" \
        --version "$VERSION" --about "${ABOUT[$P]}" --repo cubale92/90boombapbundle -o "$OUT"
  done
  echo "::endgroup::"
done
ls -la "$OUT"
