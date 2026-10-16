#!/bin/bash
# Apply docs/re/names.tsv to the existing Ghidra project and regenerate work/ghidra/pest.c.
# (First run needs the full import — see AGENTS.md.)
set -euo pipefail
cd "$(dirname "$0")/.."
source ~/tools/env.sh
"$GHIDRA_HOME/support/analyzeHeadless" work/ghidra CyberPest -process PEST.EXE -noanalysis \
  -scriptPath tools/ghidra -preScript ApplyNames.java docs/re/names.tsv \
  -postScript DecompileAll.java work/ghidra/pest.c work/ghidra/functions.tsv > work/ghidra/redecompile.log 2>&1
grep -E "applied|decompiled|ERROR" work/ghidra/redecompile.log
