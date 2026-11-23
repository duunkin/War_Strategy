import subprocess
import re
import sys

EXE = "build/Release/War_Strategy_Headless.exe"
SEED = 7919

CONFIGS = [
    ("HEAD (All ON)", []),
    ("No Safe Retreat Exit", ["--ablate-no-safe-retreat"]),
    ("No Recon Inherit", ["--ablate-no-recon-inherit"]),
    ("No Deep Push", ["--ablate-no-deep-push"]),
    ("No Target Filter", ["--ablate-no-target-filter"]),
    ("No Cmd Stab", ["--ablate-no-cmd-stab"]),
    ("No Gepard Frontline Escort", ["--ablate-no-gepard-escort"]),
    ("No Symmetry Mirroring", ["--ablate-no-symmetry"]),
    ("No Deep Push HP Check", ["--ablate-no-deep-push-hp"]),
    ("No Threat Radius Tiles", ["--ablate-no-threat-radius"]),
    ("All OFF (b9d0aef Baseline)", ["--ablate-all-off"]),
]

def run_config(name, flags):
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(SEED),
        "--max-ticks", "4800",
        "--trace-every", "1200",
        "--quiet"
    ] + flags

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    traces = {}
    current_tick = None

    for line in stdout.splitlines():
        m_trace = re.search(r"\[TRACE TICK (\d+)", line)
        if m_trace:
            current_tick = int(m_trace.group(1))
            traces[current_tick] = {"blue": 0, "red": 0}
            continue

        if current_tick is not None and ("AI1_Blue" in line or "AI2_Red" in line):
            m_troops = re.search(r"(\d+)/(\d+)", line)
            if m_troops:
                cur = int(m_troops.group(1))
                if "AI1_Blue" in line:
                    traces[current_tick]["blue"] += cur
                elif "AI2_Red" in line:
                    traces[current_tick]["red"] += cur

    return traces

def main():
    print(f"=== FIRST BATTLE TROOP COUNTS (Seed {SEED}) ===")
    print(f"{'Configuration':<30} | {'Tick 1200 (B/R/Total)':<24} | {'Tick 2400 (B/R/Total)':<24} | {'Tick 4800 (B/R/Total)':<24}")
    print("-" * 110)

    for name, flags in CONFIGS:
        traces = run_config(name, flags)
        
        def fmt(t):
            d = traces.get(t, {"blue": 0, "red": 0})
            b = d["blue"]
            r = d["red"]
            tot = b + r
            pct_lost = ((61222 - tot) / 61222.0) * 100.0 if tot > 0 else 100.0
            return f"{b:>5}/{r:>5} ({tot:>5}, -{pct_lost:4.1f}%)"

        t1200_str = fmt(1200)
        t2400_str = fmt(2400)
        t4800_str = fmt(4800)
        print(f"{name:<30} | {t1200_str:<24} | {t2400_str:<24} | {t4800_str:<24}")

if __name__ == "__main__":
    main()
