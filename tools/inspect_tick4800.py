import subprocess
import re

def inspect_run(flags, title):
    cmd = ["build/Release/War_Strategy_Headless.exe", "--matches", "1", "--seed", "7919", "--max-ticks", "4800", "--trace-every", "1200"] + flags
    p = subprocess.run(cmd, capture_output=True, text=True)
    cur_tick = None
    data = {}
    units_at_4800 = []
    
    for line in p.stdout.splitlines():
        m = re.search(r"\[TRACE TICK (\d+)", line)
        if m:
            cur_tick = int(m.group(1))
            data[cur_tick] = {"bu": 0, "btr": 0, "ru": 0, "rtr": 0}
            continue
        if cur_tick is not None and ("AI1_Blue" in line or "AI2_Red" in line):
            m2 = re.search(r"(\d+)/(\d+)", line)
            if m2:
                tr = int(m2.group(1))
                if "AI1_Blue" in line:
                    data[cur_tick]["bu"] += 1
                    data[cur_tick]["btr"] += tr
                elif "AI2_Red" in line:
                    data[cur_tick]["ru"] += 1
                    data[cur_tick]["rtr"] += tr
            if cur_tick == 4800:
                units_at_4800.append(line)

    print(f"=== {title} ===")
    for t, d in sorted(data.items()):
        print(f"Tick {t:4d}: Blue = {d['bu']:2d} units, {d['btr']:5d} troops | Red = {d['ru']:2d} units, {d['rtr']:5d} troops")
    return units_at_4800

if __name__ == "__main__":
    u_base = inspect_run(["--ablate-all-off", "--order-player-first"], "Baseline b9d0aef (PlayerFirst)")
    print("\n--- Blue Units at Tick 4800 (Baseline PlayerFirst) ---")
    for u in u_base:
        if "AI1_Blue" in u:
            print(u)
    print("\n--- Red Units at Tick 4800 (Baseline PlayerFirst) ---")
    for u in u_base:
        if "AI2_Red" in u:
            print(u)
            
    # Also check without mapgen (if flat map was used in M9.7)
    u_head = inspect_run([], "HEAD (Simultaneous)")
