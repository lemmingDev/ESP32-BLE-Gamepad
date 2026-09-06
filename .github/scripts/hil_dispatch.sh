#!/usr/bin/env bash
# Fire a repository_dispatch at the hardware-in-the-loop rig repo to run the
# BLE-HID + latency suite against a given ESP32-BLE-Gamepad ref, then block
# until that run finishes and exit with its result.
#
# Used by .github/workflows/hil.yml. See HilTesting.md for the whole picture.
#
# repository_dispatch returns no run id, so the triggered run is found by
# workflow + event + creation time: both sides serialise on a `concurrency`
# group, so at most one hil run is created in the window after the dispatch.
# The correlation_id is passed through for log traceability only.
#
# Env:
#   GH_TOKEN         PAT with Contents:write + Actions:read on $RIG_REPO
#   RIG_REPO         owner/name of the HIL rig repo
#   RIG_WORKFLOW     workflow file name in the rig repo (hil.yml)
#   LIB_REPO_URL     clone URL of the library under test
#   LIB_REF          library ref (branch / tag / SHA) to test
#   SOURCE_RUN_URL   this workflow run's URL (passed to the rig for its logs)
set -euo pipefail

: "${GH_TOKEN:?HIL_DISPATCH_TOKEN secret is not set - see .github/workflows/hil.yml}"
: "${RIG_REPO:?}" "${RIG_WORKFLOW:?}" "${LIB_REPO_URL:?}" "${LIB_REF:?}"

summary="${GITHUB_STEP_SUMMARY:-/dev/null}"
correlation_id="hil-${GITHUB_RUN_ID:-local}.${GITHUB_RUN_ATTEMPT:-1}"

# --- dispatch -------------------------------------------------------------
# `since` gates the run search below. Back-date it a little for clock skew;
# the concurrency groups keep a stale earlier run from matching.
since="$(date -u -d '-120 seconds' +%Y-%m-%dT%H:%M:%SZ)"

echo "Dispatching 'hil' -> $RIG_REPO  (lib_ref=$LIB_REF, id=$correlation_id)"
gh api "repos/$RIG_REPO/dispatches" \
  -f "event_type=hil" \
  -F "client_payload[lib_repo]=$LIB_REPO_URL" \
  -F "client_payload[lib_ref]=$LIB_REF" \
  -F "client_payload[correlation_id]=$correlation_id" \
  -F "client_payload[source_run]=${SOURCE_RUN_URL:-}"

# --- find the triggered run --------------------------------------------
run_id=""
for _ in $(seq 1 30); do
  sleep 10
  run_id="$(gh api -X GET "repos/$RIG_REPO/actions/workflows/$RIG_WORKFLOW/runs" \
    -f "event=repository_dispatch" -f "created=>$since" -f "per_page=20" \
    --jq '.workflow_runs | sort_by(.created_at) | last | .id // empty' || true)"
  [ -n "$run_id" ] && break
  echo "  ...waiting for the rig run to be created"
done
if [ -z "$run_id" ]; then
  echo "::error::No HIL rig run appeared within 5 min of the dispatch."
  exit 1
fi

run_url="https://github.com/$RIG_REPO/actions/runs/$run_id"
echo "Rig run: $run_url"

# --- wait for completion ---------------------------------------------------
status=""
conclusion=""
for _ in $(seq 1 300); do   # 300 * 30s = 2.5h ceiling
  read -r status conclusion < <(
    gh api "repos/$RIG_REPO/actions/runs/$run_id" --jq '"\(.status) \(.conclusion // "-")"' || true
  )
  echo "  rig run: status=$status conclusion=$conclusion"
  [ "$status" = "completed" ] && break
  sleep 30
done

{
  echo "## HIL"
  echo
  echo "| | |"
  echo "|---|---|"
  echo "| Library ref | \`$LIB_REF\` |"
  echo "| Rig run | [$run_id]($run_url) |"
  echo "| Result | \`${conclusion:-timeout}\` |"
} >> "$summary"

if [ "$status" != "completed" ]; then
  echo "::error::HIL rig run did not finish within 2.5h: $run_url"
  exit 1
fi

case "$conclusion" in
  success)
    echo "HIL passed: $run_url"
    ;;
  skipped|neutral)
    echo "::warning::HIL rig run was $conclusion (no hardware verdict): $run_url"
    ;;
  *)
    echo "::error::HIL failed ($conclusion): $run_url"
    exit 1
    ;;
esac
