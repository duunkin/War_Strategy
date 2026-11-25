import subprocess
import re
import sys
import math
from concurrent.futures import ThreadPoolExecutor

EXE = "build/Release/War_Strategy_Headless.exe"
SEEDS = [1000 + i * 37 for i in range(100)]

def run_single(args):
    seed, extra_flags = args
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", "12000",
        "--quiet"
    ] + extra_flags

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    winner = "UNKNOWN"
    reason = "UNKNOWN"
    ticks = 0

    for line in stdout.splitlines():
        m_end = re.search(r"\[Match #\s*(\d+)\s*END\]\s*Winner:\s*(\S+)\s*\|\s*Reason:\s*(.*?)\s*\|\s*Ticks:\s*(\d+)", line)
        if m_end:
            winner = m_end.group(2)
            reason = m_end.group(3)
            ticks = int(m_end.group(4))
            break

    return {
        "seed": seed,
        "winner": winner,
        "reason": reason,
        "ticks": ticks
    }

def run_experiment(name, extra_flags):
    tasks = [(s, extra_flags) for s in SEEDS]
    with ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(run_single, tasks))

    blue_wins = 0
    red_wins = 0
    draws = 0
    b_cmd_killed = 0
    r_cmd_killed = 0
    timeouts = 0

    for r in results:
        w = r["winner"]
        reason = r["reason"]
        t = r["ticks"]

        is_blue = "AI1" in w or "BLUE" in w or "Blue" in w
        is_red = "AI2" in w or "RED" in w or "Red" in w

        if is_blue:
            blue_wins += 1
        elif is_red:
            red_wins += 1
        else:
            draws += 1

        if "Timeout" in reason or t >= 12000:
            timeouts += 1

        if "Tu lenh cua ban da hy sinh" in reason or ("Commander" in reason and is_red):
            b_cmd_killed += 1
        elif "Tu lenh dich da bi tieu diet" in reason or ("Commander" in reason and is_blue):
            r_cmd_killed += 1

    total = len(results)
    p_b = blue_wins / total * 100.0
    p_r = red_wins / total * 100.0

    print(f"{name:<35} | {blue_wins:>2}/{red_wins:>2}/{draws:>2} (Blue {p_b:4.1f}% | Red {p_r:4.1f}%) | CmdDied B:{b_cmd_killed:>2} R:{r_cmd_killed:>2} | Timeouts:{timeouts:>2}")
    return results

def main():
    print(f"=== FACTION BIAS EXPERIMENTS (100 SEEDS: 1000 + i*37) ===")
    print(f"{'Experiment':<35} | {'Blue/Red/Draw (Winrates)':<36} | {'Cmd Dead (B/R)':<16} | Timeouts")
    print("-" * 105)

    run_experiment("1. Baseline (PlayerFirst order)", [])
    run_experiment("2a. EnemyFirst (AI & Combat)", ["--order-enemy-first"])
    run_experiment("2a. Alternate Order (Even/Odd)", ["--order-alternate"])
    run_experiment("2a. Simultaneous Combat Damage", ["--order-simultaneous"])
    run_experiment("2b. Flipped Map X-Axis", ["--map-flip-x"])

if __name__ == "__main__":
    main()
