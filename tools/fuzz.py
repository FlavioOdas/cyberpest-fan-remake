"""Random-input soak test for the remake: clicks, drags and typing over a few simulated
minutes per seed, run headlessly; reports hangs (60 s wall-clock limit) and crashes.

Usage: python3 tools/fuzz.py [first_seed] [count]      (default: seeds 1..20)
Snapshot runs use their own save (%TEMP%\\CyberPest-test), never the player's pest. A hung
run is killed by matching "--snapshot" in its command line, so a game the user is playing
is never touched. Each seed's command line must stay under Windows' 32,767 characters,
hence 1000 events.
"""
import os
import random
import re
import subprocess
import sys
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def events(seed):
    rng = random.Random(seed)
    args = ["--type", "Fuzzy\r,600"]
    t = 1500
    for _ in range(1000):
        t += rng.randint(30, 400)
        r = rng.random()
        if r < 0.6:
            args += ["--click", f"{rng.randint(0, 639)},{rng.randint(0, 479)},{t}"]
        elif r < 0.9:
            args += ["--drag", f"{rng.randint(0, 639)},{rng.randint(0, 479)},"
                               f"{rng.randint(0, 639)},{rng.randint(0, 479)},{t}"]
            t += 450
        else:
            word = rng.choice(["hello", "sit", "eat", "bad", "good", "Fuzzy", "x" * 30])
            args += ["--type", f"{word}\r,{t}"]
    return t + 5000, args


def kill_snapshots():
    subprocess.run(["powershell.exe", "-NoProfile", "-Command",
                    "Get-CimInstance Win32_Process -Filter \"Name='cyberpest.exe'\" | "
                    "Where-Object { $_.CommandLine -like '*--snapshot*' } | "
                    "ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"], capture_output=True)


def main():
    first = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    os.chdir(ROOT)
    adm = subprocess.check_output(["wslpath", "-w", "original/extracted/APP/SLOW/PEST.ADM"]).decode().strip()
    os.makedirs("work/test", exist_ok=True)
    out = subprocess.check_output(["wslpath", "-w", "work/test"]).decode().strip() + "\\fuzz.bmp"
    failures = 0
    for seed in range(first, first + count):
        duration, args = events(seed)
        subprocess.run(["cmd.exe", "/c", r"if exist %TEMP%\CyberPest-test\PEST.CFG del %TEMP%\CyberPest-test\PEST.CFG"],
                       cwd="/mnt/c", capture_output=True)
        start = time.time()
        try:
            r = subprocess.run(["./build/cyberpest.exe", "--adm", adm, "--snapshot", out, "--time", str(duration),
                                "--dump"] + args, capture_output=True, timeout=60, text=True)
        except subprocess.TimeoutExpired:
            kill_snapshots()
            failures += 1
            print(f"seed {seed}: HANG", flush=True)
            continue
        pest = next((l.split()[4] for l in r.stdout.splitlines() if re.match(r"\s*-105 ", l)), "?")
        bad = r.returncode != 0 or r.stderr.strip()
        failures += bool(bad)
        print(f"seed {seed}: exit {r.returncode} in {time.time() - start:.1f}s, pest state {pest}"
              + (f"\n  stderr: {r.stderr.strip()[:300]}" if r.stderr.strip() else ""), flush=True)
    print(f"{failures} failing seed(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
