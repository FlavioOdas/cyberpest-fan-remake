#!/bin/bash
# Build a release zip for players: cyberpest.exe + README.txt (no game data; players bring
# their own, or the first run downloads it from the Internet Archive with consent).
# Usage: tools/package.sh [version]  -> work/dist/CyberPest-remake-<version>.zip
# (version defaults to `git describe --always --dirty`).
set -euo pipefail
cd "$(dirname "$0")/.."
source ~/tools/env.sh
cmake --build build -j16 > work/build.log 2>&1 || { grep -E "error" work/build.log | head -20; exit 1; }
version="${1:-$(git describe --always --dirty)}"
mkdir -p work/dist
python3 - "$version" <<'PY'
import sys, zipfile
version = sys.argv[1]
out = f"work/dist/CyberPest-remake-{version}.zip"
readme = open("README.md", encoding="utf-8").read().replace("\n", "\r\n")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.write("build/cyberpest.exe", "CyberPest/cyberpest.exe")
    z.writestr("CyberPest/README.txt", readme)
print(out)
PY
