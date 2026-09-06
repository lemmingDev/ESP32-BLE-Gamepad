#!/usr/bin/env bash
# Run the hardware-in-the-loop suite against the current working tree.
#
# Builds the hil_runner firmware here (needs PlatformIO + a local checkout of
# the harness repo), pushes the flashable bundles to the tester (a Raspberry Pi
# + ESP32 + BLE adapter), runs the pytest BLE-HID suite + latency benchmark
# there, and pulls the results back into ./hil-results/ (gitignored).
#
# The tester never builds anything; all compilation happens here or on a CI
# runner. See HilTesting.md for the full picture.
#
#   scripts/hil.sh                                   # all boards + profiles, no bench
#   scripts/hil.sh --bench                           # + latency/throughput benchmark
#   scripts/hil.sh --boards esp32dev --profiles "default maxbtn"
#   scripts/hil.sh --profiles default -- -k buttons  # args after -- go to pytest
#
# Env (HIL_SSH_HOST / HIL_SSH_USER are required -- set them or your ssh config):
#   HIL_REPO      local ESP32-BLE-Gamepad-HIL checkout  (default: ~/src/ESP32-BLE-Gamepad-HIL)
#   HIL_SSH_HOST  tester host  (hostname / IP / Tailscale name)
#   HIL_SSH_USER  tester ssh user
#   HIL_SSH_KEY   ssh identity file (optional; else your ssh config / agent)
#   HIL_PIO       pio binary   (default: pio on $PATH)
set -euo pipefail

LIB_DIR=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
HIL_REPO=${HIL_REPO:-$HOME/src/ESP32-BLE-Gamepad-HIL}
HIL_SSH_HOST=${HIL_SSH_HOST:?set HIL_SSH_HOST to the tester host}
HIL_SSH_USER=${HIL_SSH_USER:?set HIL_SSH_USER to the tester ssh user}
PIO=${HIL_PIO:-pio}
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"
PI="$HIL_SSH_USER@$HIL_SSH_HOST"
REMOTE_DIR=ESP32-BLE-Gamepad-HIL          # harness checkout on the tester (see hil.yml)

SSH=(ssh -o BatchMode=yes); RSYNC_E=(ssh -o BatchMode=yes)
if [[ -n ${HIL_SSH_KEY:-} ]]; then SSH+=(-i "$HIL_SSH_KEY"); RSYNC_E+=(-i "$HIL_SSH_KEY"); fi

BOARDS=${HIL_BOARDS:-"esp32dev esp32c3"}
PROFILES=${HIL_PROFILES:-"default signed-axes specials minimal maxbtn reports"}
BENCH=()
PYTEST_ARGS=()
seen_ddash=0
while [[ $# -gt 0 ]]; do
  if [[ $seen_ddash == 1 ]]; then PYTEST_ARGS+=("$1"); shift; continue; fi
  case "$1" in
    --) seen_ddash=1; shift ;;
    --bench) BENCH=(--bench); shift ;;
    --boards) BOARDS=$2; shift 2 ;;
    --profiles) PROFILES=$2; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

[[ -d "$HIL_REPO/.git" ]] || {
  echo "HIL_REPO=$HIL_REPO is not a git checkout of ESP32-BLE-Gamepad-HIL" >&2
  echo "  git clone https://github.com/LeeNX/ESP32-BLE-Gamepad-HIL $HIL_REPO" >&2
  exit 2
}

echo "== library under test:  $LIB_DIR ($(git -C "$LIB_DIR" describe --tags --always --dirty))"
echo "== harness:             $HIL_REPO"
echo "== tester:              $PI"
echo "== boards/profiles:     [$BOARDS] x [$PROFILES]"

# --- 1. point the builder at this working tree + the Pi ---------------------
CFG="$HIL_REPO/hil_config.local.toml"
CFG_BAK=""
if [[ -f "$CFG" ]]; then CFG_BAK="$CFG.hil-sh.bak"; cp "$CFG" "$CFG_BAK"; fi
cleanup() {
  if [[ -n "$CFG_BAK" ]]; then mv "$CFG_BAK" "$CFG"; else rm -f "$CFG"; fi
}
trap cleanup EXIT

cat > "$CFG" <<EOF
# written by ESP32-BLE-Gamepad/scripts/hil.sh -- restored on exit
[rig]
lib_dir = "$LIB_DIR"
[tester]
ssh_host = "$HIL_SSH_HOST"
ssh_user = "$HIL_SSH_USER"
EOF

# --- 2. build the firmware bundles here ------------------------------------
# clean first -- stale bundles from an earlier lib sha would also get flashed
rm -rf "$HIL_REPO/bundles"
( cd "$HIL_REPO" && HIL_PIO="$PIO" ./builder/build.sh --boards "$BOARDS" --profiles "$PROFILES" )

# --- 3. sync harness code + bundles to the Pi ----------------------------
# The Pi keeps its own hil_config.local.toml (real serial ports) -- never touch it.
echo "== rsync harness + bundles -> $PI"
rsync -a --delete -e "${RSYNC_E[*]}" \
  --exclude '.git' --exclude '.pio' --exclude 'bundles' --exclude 'results' \
  --exclude 'hil_config.local.toml' --exclude '__pycache__' --exclude '*.pyc' \
  "$HIL_REPO"/ "$PI:$REMOTE_DIR/"
rsync -a --delete -e "${RSYNC_E[*]}" "$HIL_REPO"/bundles/ "$PI:hil-bundles/"

# --- 4. flash + test each bundle on the Pi ------------------------------
# extra args are appended straight to pytest (our --bench flag + anything the
# caller put after --, e.g. -k buttons). Per-bundle output streams live; each
# bundle prints its own phase banners + one-line verdict (tester/test.sh).
rc=0
"${SSH[@]}" "$PI" "bash -s --" "$REMOTE_DIR" "${BENCH[@]}" "${PYTEST_ARGS[@]}" <<'REMOTE' || rc=$?
set -e
REMOTE_DIR="$1"; shift
cd ~/"$REMOTE_DIR"
rm -rf results && mkdir results
total=$(ls -d ~/hil-bundles/*/ | wc -l)
echo "== $total bundle(s) to flash + test on $(hostname)"
rc=0; i=0
for b in ~/hil-bundles/*/; do
  i=$((i+1))
  echo; echo "########## [$i/$total] $(basename "$b")  $(date +%H:%M:%S)"
  ./tester/test.sh "$b" "$@" || rc=$?
done
echo; echo '########## verdicts'; cat results/run-verdicts.md 2>/dev/null || true
exit $rc
REMOTE

# --- 5. pull results + regenerate the distilled table/charts -----------
mkdir -p "$LIB_DIR/hil-results"
rsync -a -e "${RSYNC_E[*]}" "$PI:$REMOTE_DIR/results/" "$LIB_DIR/hil-results/" || true
if ls "$LIB_DIR"/hil-results/bench-*.json >/dev/null 2>&1; then
  PYTHONPATH="$HIL_REPO/host" python3 -m hil.charts "$LIB_DIR/hil-results/" || true
fi

echo
echo "=================================================================="
[[ -f "$LIB_DIR/hil-results/run-verdicts.md" ]] && cat "$LIB_DIR/hil-results/run-verdicts.md"
echo "-- results in $LIB_DIR/hil-results/  (summary.md, bench-table.md, *.svg)"
[[ $rc == 0 ]] && echo "== HIL PASS" || echo "== HIL FAIL (rc=$rc)"
exit $rc
