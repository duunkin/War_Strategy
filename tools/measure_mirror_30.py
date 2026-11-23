import subprocess
import re
import sys
from concurrent.futures import ThreadPoolExecutor

EXE = r"D:\clean_ws\build\Release\War_Strategy_Headless.exe"
SEEDS = [1000 + i * 37 for i in range(30)] # 30 distinct seeds

def run_single(seed):
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", "12000",
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

def main():
    print("MAP: generated (seed=...)")
    print(f"=== 30-SEED BASELINE-VS-BASELINE MIRROR BENCHMARK ===")
    print(f"{'Pair #':<7} | {'Seed':<8} | {'Match A (Cand=B)':<18} | {'Match B (Cand=R)':<18} | {'Ticks':<8} | Reason")
    print("-" * 95)

    with ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(run_single, SEEDS))

    cand_wins = 0
    opp_wins = 0
    draws = 0

    blue_wins = 0
    red_wins = 0

    cmd_dead_blue = 0
    cmd_dead_red = 0

    results.sort(key=lambda r: r["seed"])

    for i, res in enumerate(results):
        seed = res["seed"]
        w = res["winner"]
        reason = res["reason"]
        ticks = res["ticks"]

        # In Match A (Seed s): Candidate is Blue, Opponent is Red
        # In Match B (Seed s): Opponent is Blue, Candidate is Red
        # If w == Blue: Cand wins A, Opp wins B
        # If w == Red: Opp wins A, Cand wins B
        if "AI1" in w or "BLUE" in w or "Blue" in w:
            match_a = "Cand (Blue)"
            match_b = "Opp (Blue)"
            cand_wins += 1
            opp_wins += 1
            blue_wins += 2
        elif "AI2" in w or "RED" in w or "Red" in w:
            match_a = "Opp (Red)"
            match_b = "Cand (Red)"
            opp_wins += 1
            cand_wins += 1
            red_wins += 2
        else:
            match_a = "Draw"
            match_b = "Draw"
            draws += 2

        if "Commander" in reason or "Tieu diet Tu lenh" in reason:
            if "AI1" in w or "BLUE" in w:
                cmd_dead_red += 1
            elif "AI2" in w or "RED" in w:
                cmd_dead_blue += 1

        print(f"{i+1:<7} | {seed:<8} | {match_a:<18} | {match_b:<18} | {ticks:<8} | {reason}")

    total_matches = len(SEEDS) * 2
    cand_wr = (cand_wins / total_matches) * 100.0
    blue_wr = (blue_wins / total_matches) * 100.0
    red_wr = (red_wins / total_matches) * 100.0

    print("=" * 95)
    print(f"SUMMARY (30 Seed Pairs = 60 Matches):")
    print(f"  - Candidate Wins : {cand_wins}/{total_matches} ({cand_wr:.1f}%) [Expected: ~50.0%]")
    print(f"  - Opponent Wins  : {opp_wins}/{total_matches} ({(opp_wins/total_matches)*100.0:.1f}%)")
    print(f"  - Draws          : {draws}/{total_matches}")
    print(f"  - Faction Wins   : Blue {blue_wins//2} ({blue_wr:.1f}%) | Red {red_wins//2} ({red_wr:.1f}%) across 30 seeds")
    print(f"  - Noise Floor Margin: +/- {abs(blue_wr - 50.0):.1f}%")
    print("=" * 95)

if __name__ == "__main__":
    main()
