#!/usr/bin/env bash
# Shared paths for the build and test scripts. Sourced, not run.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPROJECT="$PROJECT_DIR/Nevergone.uproject"
ENGINE_DIR="${UE_ENGINE_DIR:-/e/Game Dev/UE_5.7}"
UBT="$ENGINE_DIR/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe"
EDITOR_CMD="$ENGINE_DIR/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"

# Windows form of a path, for the engine executables.
winpath() { cygpath -w "$1"; }

require_exe() {
  if [ ! -x "$1" ]; then
    echo "FAIL: not found: $1 (set UE_ENGINE_DIR to the UE 5.7 folder)"
    return 1
  fi
}

editor_is_running() {
  tasklist //FI "IMAGENAME eq UnrealEditor.exe" 2>/dev/null | grep -qi "UnrealEditor.exe"
}

# Runs the automation tests whose names start with $1 in a -game instance
# without a window, then reads the exported report. $2 names the run.
# Exit code 0 only when every test ran and passed.
run_automation() {
  local filter="$1" name="$2"
  local out_dir="$PROJECT_DIR/Saved/Automation/$name"
  local log="$out_dir/run.log"

  rm -rf "$out_dir"
  mkdir -p "$out_dir"

  require_exe "$EDITOR_CMD" || return 1

  # A hung instance (a dialog, a stuck load) would otherwise block forever.
  timeout 600 "$EDITOR_CMD" "$(winpath "$UPROJECT")" -game -nullrhi -unattended -nosplash -nopause \
    -ExecCmds="Automation RunTests $filter" \
    -TestExit="Automation Test Queue Empty" \
    -ReportExportPath="$(winpath "$out_dir")" \
    -abslog="$(winpath "$log")" >/dev/null 2>&1
  if [ $? -eq 124 ]; then
    echo "FAIL: $name timed out after 600 s (log: $log)"
    return 1
  fi

  local report="$out_dir/index.json"
  if [ ! -f "$report" ]; then
    echo "FAIL: no report at $report. Last log lines:"
    tail -20 "$log" 2>/dev/null
    return 1
  fi

  # First occurrence of a top-level counter in the report.
  report_count() { grep -o "\"$1\": *[0-9]*" "$report" | head -1 | grep -o '[0-9]*$'; }

  local succeeded warned failed not_run in_process
  succeeded=$(report_count succeeded)
  warned=$(report_count succeededWithWarnings)
  failed=$(report_count failed)
  not_run=$(report_count notRun)
  in_process=$(report_count inProcess)

  # Each test's result and any error it collected.
  grep -E 'Test Completed\. Result=|Error: ' "$log" | grep -E 'LogAutomationController' | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -40

  local passed=$(( ${succeeded:-0} + ${warned:-0} ))
  echo "$name: $passed passed ($warned with warnings), $failed failed, $not_run not run (log: $log)"

  # Zero tests found, any failure, or a test that never finished: fail.
  if [ "$passed" -eq 0 ] || [ "${failed:-1}" -ne 0 ] || [ "${not_run:-1}" -ne 0 ] || [ "${in_process:-1}" -ne 0 ]; then
    return 1
  fi
  return 0
}
