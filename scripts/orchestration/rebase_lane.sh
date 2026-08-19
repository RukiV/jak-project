#!/usr/bin/env bash
# Rebase a lane worktree onto a target commit, auto-resolving only jsonc list seams and the
# generated inert ledger, and refusing to stage anything the resolver could not prove.
#
# Usage: rebase_lane.sh <worktree-posix-path> <onto-sha>
#
# Ends with a per-commit validation (conflict markers + jsonc syntax) of the new range, via
# this same directory's check_commits.py. resolve_append.py (jsonc seam repair) and
# check_commits.py are resolved relative to THIS script's own location, so both must be
# committed alongside it under scripts/orchestration/; there is no scratchpad fallback.
set -u

if [ "$#" -lt 2 ]; then
  echo "usage: rebase_lane.sh <worktree> <onto-sha>" >&2
  exit 2
fi
WT="$1"; ONTO="$2"

SELF="${BASH_SOURCE[0]}"
SCRIPT_DIR=$(cd "$(dirname "$SELF")" && pwd)
RESOLVE_APPEND="$SCRIPT_DIR/resolve_append.py"
CHECK_COMMITS="$SCRIPT_DIR/check_commits.py"
for f in "$RESOLVE_APPEND" "$CHECK_COMMITS"; do
  if [ ! -f "$f" ]; then
    echo "missing sibling script: $f" >&2
    exit 2
  fi
done

cd "$WT" || { echo "cannot cd to worktree: $WT" >&2; exit 2; }

git rebase "$ONTO" 2>&1 | grep -E "Successfully|Could not|CONFLICT" | head -3
for k in $(seq 1 30); do
  if git status --short | grep -qE '^(UU|AA|DU|UD)'; then
    ok=1
    for f in $(git status --short | grep -E '^(UU|AA|DU|UD)' | awk '{print $2}'); do
      echo "conflict in $f"
      case "$f" in
        *.jsonc)
          if python "$RESOLVE_APPEND" "$f"; then git add "$f"; else ok=0; fi ;;
        docs/inert-inventory.md)
          git checkout --theirs "$f" 2>/dev/null; python scripts/gen_inert_ledger.py --write >/dev/null 2>&1 && git add "$f" ;;
        *)
          echo "NON-AUTO CONFLICT: $f"; ok=0 ;;
      esac
    done
    if [ "$ok" = 0 ]; then echo "STOPPED: resolve by hand, then 'git add' and 'GIT_EDITOR=true git rebase --continue'"; exit 3; fi
    GIT_EDITOR=true git rebase --continue 2>&1 | grep -E "Successfully|Could not|CONFLICT" | head -2
  else
    break
  fi
done
git status --short | head -5
git log --oneline -1
python "$CHECK_COMMITS" "$WT" "$ONTO..HEAD"
