import subprocess
import re
import sys
from concurrent.futures import ThreadPoolExecutor

SEEDS = [7919, 15838, 23757, 31676, 39595, 47514, 55433, 63352]
EXE = "build/Release/War_Strategy_Headless.exe"

CONFIGS = [
    ("All ON (M9.7 Full)", []),
    ("No Safe Retreat Exit", ["--ablate-no-safe-retreat"]),
    ("No Recon Inherit", ["--ablate-no-recon-inherit"]),
    ("No Deep Push", ["--ablate-no-deep-push"]),
    ("No Target Filter", ["--ablate-no-target-filter"]),
    ("No Cmd Stab", ["--ablate-no-cmd-stab"]),
    ("All OFF (M9.6 Baseline)", ["--ablate-all-off"])
]

def run_seed_task(args):
    seed, extra_flags, max_ticks = args
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", str(max_ticks),
        "--trace-every", "2400",
        "--quiet"
    ] + extra_flags
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    ticks_data = {}
    current_tick = None

    for line in stdout.splitlines():
        m_trace = re.search(r"\[TRACE TICK (\d+)", line)
        if m_trace:
            current_tick = int(m_trace.group(1))
            ticks_data[current_tick] = {"ai1_troops": 0, "ai2_troops": 0}
            continue

        if current_tick is not None and ("AI1_Blue" in line or "AI2_Red" in line):
            m_troops = re.search(r"(\d+)/(\d+)", line)
            if m_troops:
                cur_troops = int(m_troops.group(1))
                if "AI1_Blue" in line:
                    ticks_data[current_tick]["ai1_troops"] += cur_troops
                elif "AI2_Red" in line:
                    ticks_data[current_tick]["ai2_troops"] += cur_troops

        m_end = re.search(r"\[Match #\s*(\d+)\s*END\]\s*Winner:\s*(\S+)\s*\|\s*Reason:\s*(.*?)\s*\|\s*Ticks:\s*(\d+)", line)
        if m_end:
            winner = m_end.group(2)
            reason = m_end.group(3)
            end_tick = int(m_end.group(4))
            return {
                "seed": seed,
                "winner": winner,
                "reason": reason,
                "end_tick": end_tick,
                "traces": ticks_data
            }

    return {
        "seed": seed,
        "winner": "UNKNOWN",
        "reason": "Process exited without match end",
        "end_tick": 0,
        "traces": ticks_data
    }

def main():
    print(f"{'Config':<25} | {'Unfrozen':<10} | {'Cmd Dead':<10} | {'Blue/Red/Draw':<15} | Details")
    print("-" * 85)

    with ThreadPoolExecutor(max_workers=8) as executor:
        for name, flags in CONFIGS:
            tasks = [(seed, flags, 24000) for seed in SEEDS]
            results = list(executor.map(run_seed_task, tasks))

            unfrozen_count = 0
            cmd_dead_count = 0
            blue_wins = 0
            red_wins = 0
            draws = 0
            details = []

            # Sort by seed order
            results.sort(key=lambda r: r["seed"])

            for res in results:
                t7200 = res["traces"].get(7200, {"ai1_troops": "?", "ai2_troops": "?"})
                t12000 = res["traces"].get(12000, {"ai1_troops": "?", "ai2_troops": "?"})
                troops_changed = (t7200 != t12000)
                ended_early = (res["end_tick"] < 24000)
                is_unfrozen = (ended_early or troops_changed)

                if is_unfrozen:
                    unfrozen_count += 1

                if "Commander" in res["reason"] or "Elimination" in res["reason"]:
                    cmd_dead_count += 1

                if "AI1" in res["winner"] or "Blue" in res["winner"]:
                    blue_wins += 1
                elif "AI2" in res["winner"] or "Red" in res["winner"]:
                    red_wins += 1
                else:
                    draws += 1

                status_char = "P" if is_unfrozen else "F"
                details.append(f"{res['seed']}:{status_char}")

            win_str = f"{blue_wins}/{red_wins}/{draws}"
            unfrozen_str = f"{unfrozen_count}/8"
            cmd_dead_str = f"{cmd_dead_count}/8"
            print(f"{name:<25} | {unfrozen_str:<10} | {cmd_dead_str:<10} | {win_str:<15} | {' '.join(details)}")

if __name__ == "__main__":
    main()
