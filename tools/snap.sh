#!/bin/bash
# Build, run the remake headlessly on Windows (via WSL interop) and convert the frame to PNG.
# Usage: tools/snap.sh [extra cyberpest.exe args]   -> work/test/snap.png
set -euo pipefail
cd "$(dirname "$0")/.."
source ~/tools/env.sh
mkdir -p work/test
if ! cmake --build build -j16 > work/build.log 2>&1; then grep -E "error" work/build.log | head -20; exit 1; fi
grep -E "warning" work/build.log | head -10 || true
./build/cyberpest.exe --adm "$(wslpath -w original/extracted/APP/SLOW/PEST.ADM)" \
  --snapshot "$(wslpath -w work/test)\\snap.bmp" "$@"
python3 - <<'PY'
import struct, sys
sys.path.insert(0, "tools")
from png import write_indexed
b = open("work/test/snap.bmp", "rb").read()
w, h = struct.unpack_from("<ii", b, 18)
off = struct.unpack_from("<I", b, 10)[0]
pal = [(b[54 + 4 * i + 2], b[54 + 4 * i + 1], b[54 + 4 * i]) for i in range(256)]
st = (w + 3) & ~3
write_indexed("work/test/snap.png", w, h, b"".join(b[off + y * st:off + y * st + w] for y in reversed(range(h))), pal)
print("work/test/snap.png", w, h)
PY
