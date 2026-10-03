#!/usr/bin/env bash
# Builds NevergoneEditor Win64 Development. Refuses to run with the editor
# open: Live Coding holds the module and the external build fails.
# Prints only errors, warnings and the result; the full output goes to
# Saved/Automation/build.log.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if editor_is_running; then
  echo "FAIL: Unreal Editor is open. Close it before building."
  exit 1
fi

require_exe "$UBT" || exit 1

mkdir -p "$PROJECT_DIR/Saved/Automation"

"$UBT" NevergoneEditor Win64 Development -Project="$(winpath "$UPROJECT")" -WaitMutex 2>&1 \
  | tee "$PROJECT_DIR/Saved/Automation/build.log" \
  | grep -E ': (fatal )?(error|warning)|Error:|Warning:|Result:|Total execution time'
exit "${PIPESTATUS[0]}"
