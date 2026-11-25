import subprocess
import re
import sys

SEEDS = [7919, 15838, 23757, 31676, 39595, 47514, 55433, 63352]
EXE = "build/Release/War_Strategy_Headless.exe"

def run_seed(seed, max_ticks=24000):
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", str(max_ticks),
        "--trace-every", "2400",
        "--quiet"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    # Parse trace outputs
    # Example trace header: [TRACE TICK 7200 (360s)] Living Units: 24
    # Followed by unit lines with Troops, Faction, etc.
    # Also parse Match End: [Match # 1 END] Winner: AI2        | Reason: Timeout (Max Ticks)! ... | Ticks: 24000 (1200s)
    
    ticks_data = {}
    current_tick = None

    for line in stdout.splitlines():
        m_trace = re.search(r"\[TRACE TICK (\d+)", line)
        if m_trace:
            current_tick = int(m_trace.group(1))
            ticks_data[current_tick] = {"ai1_troops": 0, "ai2_troops": 0}
            continue

        if current_tick is not None and ("AI1_Blue" in line or "AI2_Red" in line):
            # troops format: 850/1000 or similar
            m_troops = re.search(r"(\d+)/(\d+)", line)
            if m_troops:
                cur_troops = int(m_troops.group(1))
                if "AI1_Blue" in line:
                    ticks_data[current_tick]["ai1_troops"] += cur_troops
                elif "AI2_Red" in line:
                    ticks_data[current_tick]["ai2_troops"] += cur_troops

        m_end = re.search(r"\[Match #\s*(\d+)\s*END\]\s*Winner:\s*(\S+)\s*\|\s*Reason:\s*(.*?)\s*\|\s*Ticks:\s*(\d+)", line)
        if m_end:
            match_id = int(m_end.group(1))
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

if __name__ == "__main__":
    seeds_to_run = SEEDS if len(sys.argv) <= 1 else [int(x) for x in sys.argv[1:]]
    seeds_str = ",".join(str(s) for s in seeds_to_run)
    print(f"MAP: generated (seed={seeds_str})")
    for s in seeds_to_run:
        res = run_seed(s)
        t7200 = res["traces"].get(7200, {"ai1_troops": "?", "ai2_troops": "?"})
        t12000 = res["traces"].get(12000, {"ai1_troops": "?", "ai2_troops": "?"})
        troops_changed = (t7200 != t12000)
        ended_early = (res["end_tick"] < 24000)
        status = "PASS" if (ended_early or troops_changed) else "FAIL"
        print(f"Seed {s}: EndTick={res['end_tick']} Winner={res['winner']} Reason='{res['reason']}'")
        print(f"   Tick 7200: AI1={t7200['ai1_troops']} AI2={t7200['ai2_troops']} | Tick 12000: AI1={t12000['ai1_troops']} AI2={t12000['ai2_troops']}")
        print(f"   Troops Changed (7200->12000): {troops_changed} | Ended Early (<24000): {ended_early} => Status: {status}")
