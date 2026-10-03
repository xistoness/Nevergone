#!/usr/bin/env bash
# Runs every automation test named Nevergone.* (logic tests and the smoke
# test) without opening the editor window.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

run_automation "Nevergone." "Tests"
