#!/usr/bin/env bash
# Smoke test only: opens the excursion test level, runs it for a few
# seconds and fails on any Error or ensure in the log.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

run_automation "Nevergone.Smoke" "Smoke"
