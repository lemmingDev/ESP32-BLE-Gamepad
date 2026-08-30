#!/usr/bin/env bash
# Run the hardware-in-the-loop suite against the current working tree.
#
# Builds the hil_runner firmware here (needs PlatformIO + a local checkout of
# the harness repo), pushes the flashable bundles to the Raspberry Pi tester,
# runs the pytest BLE-HID suite + latency benchmark there, and pulls the
# results back into ./hil-results/ (gitignored).
#
# The Pi never builds anything (it's a 3B+); all compilation happens here or on
# a CI runner. See HilTesting.md for the full picture.
#
#   scripts/hil.sh                                   # all boards + profiles, no bench
#   scripts/hil.sh --bench                           # + latency/throughput benchmark
#   scripts/hil.sh --boards esp32dev --profiles "default maxbtn"
#   scripts/hil.sh --profiles default -- -k buttons  # args after -- go to pytest
#
# Env:
#   HIL_REPO      local esp32-ble-gamepad-hil checkout   (default: ~/src/esp32-ble-gamepad-hil)
#   HIL_SSH_HOST  tester host  (default: 192.168.101.16; a Tailscale name works)
#   HIL_SSH_USER  tester user  (default: bot-gitea-esp32-hil)
#   HIL_SSH_KEY   ssh identity file (optional; else your ssh config / agent)
#   HIL_PIO       pio binary   (default: ~/.local/bin/pio, or $PATH)
set -euo pipefail

LIB_DIR=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
HIL_REPO=${HIL_REPO:-$HOME/src/esp32-ble-gamepad-hil}
HIL_SSH_HOST=${HIL_SSH_HOST:-192.168.101.16}
HIL_SSH_USER=${HIL_SSH_USER:-bot-gitea-esp32-hil}
PIO=${HIL_PIO:-$HOME/.local/bin/pio}
command -v "$PIO" >/dev/null 2>&1 || PIO=pio
PI="$HIL_SSH_USER@$HIL_SSH_HOST"
REMOTE_DIR=esp32-ble-gamepad-hil          # harness checkout on the Pi (see hil.yml)

SSH=(ssh -o BatchMode=yes); RSYNC_E=(ssh -o BatchMode=yes)
if [[ -n ${HIL_SSH_KEY:-} ]]; then SSH+=(-i "$HIL_SSH_KEY"); RSYNC_E+=(-i "$HIL_SSH_KEY"); fi

BOARDS=${HIL_BOARDS:-"esp32dev esp32c3"}
PROFILES=${HIL_PROFILES:-"default signed-axes specials minimal maxbtn"}
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
  echo "HIL_REPO=$HIL_REPO is not a git checkout of esp32-ble-gamepad-hil" >&2
  echo "  git clone ssh://git@gitea.h.leenx.nz:2222/leenx-foss/esp32-ble-gamepad-hil.git $HIL_REPO" >&2
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
# caller put after --, e.g. -k buttons).
EXTRA="${BENCH[*]:-} ${PYTEST_ARGS[*]:-}"
rc=0
"${SSH[@]}" "$PI" "set -e
  cd ~/$REMOTE_DIR
  rm -rf results && mkdir results
  rc=0
  for b in ~/hil-bundles/*/; do
    ./tester/test.sh \"\$b\" $EXTRA || rc=\$?
  done
  exit \$rc" || rc=$?

# --- 5. pull results + regenerate the distilled table/charts -----------
mkdir -p "$LIB_DIR/hil-results"
rsync -a -e "${RSYNC_E[*]}" "$PI:$REMOTE_DIR/results/" "$LIB_DIR/hil-results/" || true
if ls "$LIB_DIR"/hil-results/bench-*.json >/dev/null 2>&1; then
  PYTHONPATH="$HIL_REPO/host" python3 -m hil.charts "$LIB_DIR/hil-results/" || true
fi

echo
echo "== results in $LIB_DIR/hil-results/  (summary.md, bench-table.md, *.svg)"
[[ -f "$LIB_DIR/hil-results/summary.md" ]] && cat "$LIB_DIR/hil-results/summary.md"
exit $rc
