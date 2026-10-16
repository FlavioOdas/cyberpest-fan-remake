#!/bin/bash
# Recreate original/ (gitignored) from the user's CyberPest install media.
# Usage: tools/bootstrap.sh [path/to/CyberPest.zip]
set -euo pipefail
cd "$(dirname "$0")/.."
ZIP="${1:-/mnt/c/Users/odasf/Downloads/CyberPest.zip}"
mkdir -p original/installer
unzip -oq "$ZIP" -d original/installer || true   # the zip has a bogus "/" entry; unzip warns and continues
python3 tools/is3x.py original/installer/DATA.Z original/extracted
