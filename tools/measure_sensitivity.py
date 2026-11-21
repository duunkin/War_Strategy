#!/usr/bin/env python3
import json
import os
import subprocess
import tempfile
import sys
import csv

BASELINE = {
    "aggression": 0.75,
    "artilleryIdealRangeRatio": 0.80,
    "artilleryStandOffMargin": 3.0,
    "commanderSafeRadiusTiles": 8.0,
    "decisionIntervalTicks": 20,
    "enableCommanderRetreat": True,
    "enableUnitRetreat": True,
    "focusFireWeight": 1.2,
    "retreatHpRatio": 0.30,
    "scoutingGroupCount": 3,
    "scoutingWeight": 1.0,
    "targetMemoryTicks": 100,
}

PERTURBATIONS = {
    "aggression": 0.20,
    "artilleryStandOffMargin": 6.0,
    "focusFireWeight": 3.0,
    "scoutingWeight": 2.2,
    "enableCommanderRetreat": False,
    "enableUnitRetreat": False,
    "retreatHpRatio": 0.60,
    "commanderSafeRadiusTiles": 16.0,
    "artilleryIdealRangeRatio": 0.55,
    "decisionIntervalTicks": 35,
    "targetMemoryTicks": 300,
    "scoutingGroupCount": 1,
}

def run_match(ai1_params, seed=12345, max_ticks=2400):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f_ai1, \
         tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as f_csv:
        ai1_path = f_ai1.name
        csv_path = f_csv.name
        json.dump({"params": ai1_params}, f_ai1)
    
    try:
        cmd = [
            r".\build\Release\War_Strategy_Headless.exe",
            "--matches", "1",
            "--seed", str(seed),
            "--max-ticks", str(max_ticks),
            "--ai1-params", ai1_path,
            "--quiet",
            "--log", csv_path
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print("Error running headless:", res.stderr)
            return None
        
        with open(csv_path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                return {
                    "ticks": int(row["ticks"]),
                    "ai1_units": int(row["ai1_units"]),
                    "ai1_troops": int(row["ai1_troops"]),
                    "ai1_cmd_hp": int(row["ai1_cmd_hp"]),
                    "ai2_troops": int(row["ai2_troops"]),
                    "ai1_cmds": int(row["ai1_cmds"]),
                    "ai2_cmds": int(row["ai2_cmds"])
                }
    finally:
        if os.path.exists(ai1_path):
            os.remove(ai1_path)
        if os.path.exists(csv_path):
            os.remove(csv_path)
    return None

def run_batch(ai1_params, seeds=[42, 99999], max_ticks=3000):
    total_cmds = 0
    total_troops = 0
    total_ticks = 0
    for s in seeds:
        res = run_match(ai1_params, seed=s, max_ticks=max_ticks)
        if not res:
            return None
        total_cmds += res["ai1_cmds"]
        total_troops += res["ai1_troops"]
        total_ticks += res["ticks"]
    return {
        "cmds": total_cmds,
        "troops": total_troops,
        "ticks": total_ticks
    }

def main():
    print("=" * 88)
    print("      WAR STRATEGY - PARAMETER SENSITIVITY BENCHMARK (HEADLESS)")
    print("=" * 88)
    print(f"{'Parameter':<26} | {'Baseline':<8} | {'Tested':<8} | {'Delta Cmds':<11} | {'Delta Troops':<13} | {'Status':<8}")
    print("-" * 88)

    base_res = run_batch(BASELINE, seeds=[42, 99999], max_ticks=3000)
    if not base_res:
        print("Failed to run baseline simulation!")
        sys.exit(1)

    for param, test_val in PERTURBATIONS.items():
        perturbed = dict(BASELINE)
        perturbed[param] = test_val
        test_res = run_batch(perturbed, seeds=[42, 99999], max_ticks=3000)
        if not test_res:
            print(f"{param:<26} | ERROR")
            continue
        
        d_cmds = test_res["cmds"] - base_res["cmds"]
        d_troops = test_res["troops"] - base_res["troops"]
        d_ticks = test_res["ticks"] - base_res["ticks"]
        
        has_effect = (d_cmds != 0 or d_troops != 0 or d_ticks != 0)
        status = "ACTIVE" if has_effect else "PASSIVE"

        base_str = str(BASELINE[param])
        test_str = str(test_val)
        d_cmds_str = f"{d_cmds:+d}"
        d_troops_str = f"{d_troops:+d}"

        print(f"{param:<26} | {base_str:<8} | {test_str:<8} | {d_cmds_str:<11} | {d_troops_str:<13} | {status:<8}")

    print("=" * 88)

if __name__ == "__main__":
    main()
