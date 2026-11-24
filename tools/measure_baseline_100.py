import subprocess
import re
import sys
import math
import argparse
from concurrent.futures import ThreadPoolExecutor

EXE = "build/Release/War_Strategy_Headless.exe"

def run_seed_task(args):
    seed, max_ticks = args
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", str(max_ticks),
        "--quiet"
    ]
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

def wilson_score_interval(successes, total, confidence=0.95):
    if total == 0:
        return 0.0, 0.0, 0.0
    p_hat = successes / total
    z = 1.95996 # 95% CI
    denom = 1.0 + (z**2) / total
    center = (p_hat + (z**2) / (2.0 * total)) / denom
    margin = (z / denom) * math.sqrt((p_hat * (1.0 - p_hat) / total) + (z**2) / (4.0 * total**2))
    return p_hat, max(0.0, center - margin), min(1.0, center + margin)

def main():
    parser = argparse.ArgumentParser(description="Baseline vs Baseline 100-seed Non-Mirror Benchmark")
    parser.add_argument("--seeds", type=int, default=100, help="Number of seeds (default: 100)")
    parser.add_argument("--start-seed", type=int, default=1001, help="Start seed (default: 1001)")
    parser.add_argument("--max-ticks", type=int, default=12000, help="Max ticks (default: 12000)")
    parser.add_argument("--workers", type=int, default=8, help="Parallel worker threads")
    args = parser.parse_args()

    seeds = [args.start_seed + i for i in range(args.seeds)]
    print(f"MAP: generated (seed={args.start_seed}..{args.start_seed + args.seeds - 1})")
    print(f"Running {args.seeds} seeds baseline vs baseline (maxTicks={args.max_ticks}, non-mirror)...")

    tasks = [(s, args.max_ticks) for s in seeds]
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        results = list(executor.map(run_seed_task, tasks))

    results.sort(key=lambda r: r["seed"])

    blue_wins = 0
    red_wins = 0
    draws = 0

    blue_cmd_killed = 0  # Blue commander died (Red won by killing Cmdr)
    red_cmd_killed = 0   # Red commander died (Blue won by killing Cmdr)
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

        if "Timeout" in reason or t >= args.max_ticks:
            timeouts += 1

        # Check commander deaths
        # In WorldSim:
        # If Player Cmdr dies: "Tu lenh cua ban da hy sinh! That bai." -> Red wins, Blue Cmdr killed.
        # If Enemy Cmdr dies: "Tu lenh dich da bi tieu diet! Chien thang!" -> Blue wins, Red Cmdr killed.
        if "Tu lenh cua ban da hy sinh" in reason or ("Commander" in reason and is_red):
            blue_cmd_killed += 1
        elif "Tu lenh dich da bi tieu diet" in reason or ("Commander" in reason and is_blue):
            red_cmd_killed += 1

    total = len(results)
    p_b, b_lo, b_hi = wilson_score_interval(blue_wins, total)
    p_r, r_lo, r_hi = wilson_score_interval(red_wins, total)
    p_d, d_lo, d_hi = wilson_score_interval(draws, total)

    print("\n" + "=" * 80)
    print(f"BASELINE VS BASELINE (100 SEEDS, NON-MIRROR) BENCHMARK RESULTS")
    print("=" * 80)
    print(f"Total Matches       : {total}")
    print(f"Blue (AI1) Wins     : {blue_wins}/{total} ({p_b*100:.1f}%) | 95% CI: [{b_lo*100:.1f}%, {b_hi*100:.1f}%]")
    print(f"Red  (AI2) Wins     : {red_wins}/{total} ({p_r*100:.1f}%) | 95% CI: [{r_lo*100:.1f}%, {r_hi*100:.1f}%]")
    print(f"Draws               : {draws}/{total} ({p_d*100:.1f}%) | 95% CI: [{d_lo*100:.1f}%, {d_hi*100:.1f}%]")
    print("-" * 80)
    print(f"Blue Cmdr Killed    : {blue_cmd_killed}/{total} ({blue_cmd_killed/total*100:.1f}%)")
    print(f"Red Cmdr Killed     : {red_cmd_killed}/{total} ({red_cmd_killed/total*100:.1f}%)")
    print(f"Timeouts (MaxTicks) : {timeouts}/{total} ({timeouts/total*100:.1f}%)")
    print("=" * 80)

if __name__ == "__main__":
    main()
