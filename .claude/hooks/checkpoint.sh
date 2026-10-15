#!/usr/bin/env bash
# SessionStart hook: commits whatever is pending before Claude touches the
# project, so every session starts from a point that can be restored.
# Whatever this prints is shown to Claude at the start of the session.

cd "$CLAUDE_PROJECT_DIR" || exit 0

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "checkpoint: no git repository; no checkpoint was made"
  exit 0
fi

# Without an Unreal .gitignore, 'git add -A' would commit build output.
if ! grep -qi "binaries" .gitignore 2>/dev/null || ! grep -qi "intermediate" .gitignore 2>/dev/null; then
  echo "checkpoint: .gitignore does not exclude Binaries/Intermediate; no checkpoint was made"
  exit 0
fi

if [ -z "$(git status --porcelain)" ]; then
  echo "checkpoint: working tree clean; nothing to commit"
  exit 0
fi

# main only receives approved work: never checkpoint onto it (or onto a
# detached HEAD); park the pending changes on their own branch instead.
branch="$(git branch --show-current)"
if [ -z "$branch" ] || [ "$branch" = "main" ] || [ "$branch" = "master" ]; then
  stamp="$(date '+%Y%m%d-%H%M')"
  if ! git switch -c "checkpoint/$stamp" >/dev/null 2>&1; then
    echo "checkpoint: could not create branch checkpoint/$stamp; no checkpoint was made"
    exit 0
  fi
  echo "checkpoint: session started on '${branch:-detached HEAD}' with pending changes; moved them to branch checkpoint/$stamp"
  branch="checkpoint/$stamp"
fi

git add -A
if git commit -q -m "Checkpoint: before Claude Code session $(date '+%Y-%m-%d %H:%M')"; then
  echo "checkpoint: pending changes committed as $(git rev-parse --short HEAD) on $branch"
else
  echo "checkpoint: commit failed on $branch; no checkpoint was made"
fi

exit 0
