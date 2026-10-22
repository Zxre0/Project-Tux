#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Update Project Tux to the latest main from GitHub and rebuild it.
#   ./update.sh          download, rebuild (launcher, CLI, tests, Windows helpers)
#   ./update.sh --check  only report; exit 0 when up to date, 100 when updates exist
# The launcher's Settings → Updates runs this same script.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
BRANCH=main
CHECK=false
case ${1:-} in
    --check) CHECK=true ;;
    '') ;;
    -h | --help) sed -n '3,6p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $1 (try --help)" >&2; exit 2 ;;
esac

die() { echo "$*" >&2; exit 1; }
command -v git >/dev/null || die "git is not installed."
git rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
    die "This folder isn't a git checkout; download Project Tux again with git clone."
current=$(git symbolic-ref --quiet --short HEAD || echo "a detached HEAD")
[[ $current == "$BRANCH" ]] ||
    die "You're on $current, updates come from $BRANCH. Switch with: git switch $BRANCH"

git fetch --quiet origin "$BRANCH" || die "Couldn't reach GitHub. Check your internet connection."
old=$(git rev-parse HEAD)
new=$(git rev-parse FETCH_HEAD)
behind=$(git rev-list --count "$old..$new")
ahead=$(git rev-list --count "$new..$old")

if ((behind == 0)); then
    echo "Up to date ($(git rev-parse --short HEAD))."
    exit 0
fi
((behind == 1)) && noun=update || noun=updates
echo "$behind $noun available:"
git log --format='  %h %s' "$old..$new"
$CHECK && exit 100

((ahead == 0)) ||
    die "Your $BRANCH has $ahead commit(s) GitHub doesn't, so it can't be updated automatically."
if ! git diff --quiet HEAD --; then
    echo "You changed these files, so they weren't updated:" >&2
    git diff --name-only HEAD -- | sed 's/^/  /' >&2
    die "Keep them with 'git stash', or undo them with 'git checkout -- .', then run this again."
fi

echo
echo "Updating $(git rev-parse --short "$old") → $(git rev-parse --short "$new")…"
git merge --quiet --ff-only "$new"
changed() { ! git diff --quiet "$old" "$new" -- "$@"; }

echo "Building the launcher and CLI…"
./build.sh
if changed helpers/windows || [[ ! -f build/windows/fol-match-router.dll ]]; then
    echo "Building the Windows helpers…"
    bash helpers/windows/build.sh
fi

echo
echo "Project Tux is up to date ($(git rev-parse --short HEAD))."
changed integrations/windows &&
    echo "Note: the game server changed. Rebuild Erbium: see docs/MATCH.md#one-time-build"
changed integrations/lawin/prepare.py &&
    echo "Note: the backend setup changed. Prepare Lawin again: see integrations/lawin/README.md"
echo "Restart the launcher to use the new version."
