#!/bin/bash
# Install dev-only tools into ~/tools (no sudo): JDK 21, Ghidra (decompiler for the 16-bit EXE),
# llvm-mingw (cross-compiles the remake to a native Windows exe), CMake.
# The finished game needs none of these. Re-running skips tools already present.
set -euo pipefail
T="${TOOLS_DIR:-$HOME/tools}"
mkdir -p "$T" && cd "$T"

fetch() { # url dir
  if [ -d "$2" ]; then echo "have $2"; return; fi
  local f; f="$(basename "${1%%\?*}")"
  curl -fL --retry 3 -o "$f" "$1"
  case "$f" in
    *.zip) python3 -m zipfile -e "$f" . ;;
    *) tar -xf "$f" ;;
  esac
  rm -f "$f"
  [ -d "$2" ] || { echo "expected $2 after extracting $f" >&2; exit 1; }
}

fetch "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.12.1%2B1/OpenJDK21U-jdk_x64_linux_hotspot_21.0.12.1_1.tar.gz" jdk-21.0.12.1+1
fetch "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20260921.zip" ghidra_12.1.4_PUBLIC
fetch "https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64.tar.xz" llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
fetch "https://github.com/Kitware/CMake/releases/download/v4.4.4/cmake-4.4.4-linux-x86_64.tar.gz" cmake-4.4.4-linux-x86_64
# python's zipfile drops exec bits
chmod +x ghidra_12.1.4_PUBLIC/support/* ghidra_12.1.4_PUBLIC/ghidraRun 2>/dev/null || true
find ghidra_12.1.4_PUBLIC -path '*/os/linux_x86_64/*' -type f -exec chmod +x {} + 2>/dev/null || true

cat > "$T/env.sh" <<EOF
export JAVA_HOME="$T/jdk-21.0.12.1+1"
export GHIDRA_HOME="$T/ghidra_12.1.4_PUBLIC"
export PATH="\$JAVA_HOME/bin:$T/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin:$T/cmake-4.4.4-linux-x86_64/bin:\$PATH"
EOF
echo "done; run: source $T/env.sh"
