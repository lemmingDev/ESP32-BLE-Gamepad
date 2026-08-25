#!/usr/bin/env bash
# Bumps library.properties/library.json to a new version, commits, and tags
# it - the local half of the process documented in RELEASE.md. Requires a
# clean working tree so the version-bump commit only ever contains the
# version bump.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/release.sh [VERSION] [--remote NAME] [--push] [--dry-run]

Bump library.properties and library.json to VERSION, commit "Bump version
to VERSION", and create annotated tag vVERSION. Requires a clean git
working tree to start.

VERSION can be given as the first argument or via the RELEASE_VERSION
environment variable (the argument wins if both are set). Format:
MAJOR.MINOR.PATCH, optionally with a semver pre-release suffix, e.g.
0.7.7 or 0.7.7-rc0. A suffixed version tags a GitHub prerelease.

Options:
  --remote NAME  Git remote to push to (with --push, or to name in the
                 printed "next steps" otherwise). Not necessarily "origin"
                 - e.g. a fork checked out with `gh repo clone` typically
                 has no "origin" remote at all. Can also be set via the
                 RELEASE_REMOTE environment variable (the flag wins if
                 both are set). Default: the current branch's upstream
                 tracking remote if it has one, else "origin" if that
                 remote exists, else the sole remote if there's exactly
                 one - otherwise you must specify it explicitly.
  --push         Also push the current branch and the new tag.
                 Default: left for you to run yourself (printed at the end).
  --dry-run      Show what would happen without changing anything.
  -h, --help     Show this help.
EOF
}

push=false
dry_run=false
version="${RELEASE_VERSION:-}"
remote="${RELEASE_REMOTE:-}"

while [ $# -gt 0 ]; do
  case "$1" in
    --push) push=true; shift ;;
    --dry-run) dry_run=true; shift ;;
    --remote)
      if [ $# -lt 2 ] || [ -z "$2" ]; then
        echo "error: --remote requires a value" >&2
        exit 1
      fi
      remote="$2"
      shift 2
      ;;
    --remote=*) remote="${1#*=}"; shift ;;
    -h|--help) usage; exit 0 ;;
    -*)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
    *) version="$1"; shift ;;
  esac
done

if [ -z "$version" ]; then
  echo "error: no version given - pass it as an argument or set RELEASE_VERSION" >&2
  usage >&2
  exit 1
fi

if ! [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]; then
  echo "error: version '$version' doesn't look like MAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH-prerelease (e.g. 0.7.7 or 0.7.7-rc0)" >&2
  exit 1
fi

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

if [ -z "$remote" ]; then
  tracking_remote="$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null | cut -d/ -f1 || true)"
  if [ -n "$tracking_remote" ]; then
    remote="$tracking_remote"
  elif git remote | grep -qx origin; then
    remote="origin"
  elif [ "$(git remote | wc -l | tr -d ' ')" = "1" ]; then
    remote="$(git remote)"
  fi
fi

if [ -z "$remote" ]; then
  echo "error: couldn't determine which git remote to use." >&2
  echo "Pass --remote <name>, set RELEASE_REMOTE, or set an upstream tracking branch (git branch --set-upstream-to=<remote>/<branch>). Configured remotes:" >&2
  git remote -v >&2
  exit 1
fi

if ! git remote | grep -qx "$remote"; then
  echo "error: remote '$remote' is not configured. Configured remotes:" >&2
  git remote -v >&2
  exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
  echo "error: working tree is not clean - commit, stash, or discard changes first:" >&2
  git status --short >&2
  exit 1
fi

tag="v$version"

if git rev-parse "$tag" >/dev/null 2>&1; then
  echo "error: tag $tag already exists" >&2
  exit 1
fi

props_file="library.properties"
json_file="library.json"

current_props_version=$(grep -m1 '^version=' "$props_file" | cut -d= -f2)
current_json_version=$(python3 -c "import json; print(json.load(open('$json_file'))['version'])")

echo "Current version: library.properties=$current_props_version, library.json=$current_json_version"
echo "New version:     $version"
echo "Remote:          $remote"

if $dry_run; then
  echo "(dry run) would update $props_file and $json_file, commit, and tag $tag"
  exit 0
fi

sed -i.bak "s/^version=.*/version=$version/" "$props_file" && rm -f "$props_file.bak"

python3 - "$json_file" "$version" <<'PYEOF'
import json
import sys

path, version = sys.argv[1], sys.argv[2]
with open(path) as f:
    data = json.load(f)
data["version"] = version
with open(path, "w") as f:
    json.dump(data, f, indent=2)
    f.write("\n")
PYEOF

git add "$props_file" "$json_file"
git commit -m "Bump version to $version"
git tag -a "$tag" -m "$tag"

branch="$(git rev-parse --abbrev-ref HEAD)"

echo
echo "Committed and tagged $tag locally on $branch."

if $push; then
  git push "$remote" "$branch"
  git push "$remote" "$tag"
  echo "Pushed $branch and $tag to $remote."
else
  echo "Next steps:"
  echo "  git push $remote $branch"
  echo "  git push $remote $tag"
fi
