#!/usr/bin/env python3
import json
import os
import subprocess
import tempfile
import sys
import csv
import argparse

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
    "commanderSafeRadiusTiles": 18.0,
    "enableCommanderRetreat": False,
    "enableUnitRetreat": False,
    "retreatHpRatio": 0.60,
    "artilleryIdealRangeRatio": 0.55,
    "decisionIntervalTicks": 35,
    "targetMemoryTicks": 300,
    "scoutingGroupCount": 1,
}

DEFAULT_SEEDS = [12345, 67890, 42, 99999, 10101, 20202, 30303, 40404]

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

def main():
    parser = argparse.ArgumentParser(description="War Strategy - Parameter Sensitivity Benchmark")
    parser.add_argument("--seeds", type=int, default=8, help="Number of seeds to evaluate (>=8)")
    parser.add_argument("--max-ticks", type=int, default=2400, help="Max ticks per match")
    args = parser.parse_args()

    num_seeds = max(1, args.seeds)
    if num_seeds <= len(DEFAULT_SEEDS):
        seeds = DEFAULT_SEEDS[:num_seeds]
    else:
        seeds = list(DEFAULT_SEEDS)
        for i in range(len(DEFAULT_SEEDS), num_seeds):
            seeds.append(50000 + i * 1111)

    seeds_str = ",".join(str(s) for s in seeds)
    print(f"MAP: generated (seed={seeds_str})")
    print("=" * 105)
    print(f"      WAR STRATEGY - PARAMETER SENSITIVITY BENCHMARK ({len(seeds)} SEEDS)")
    print("=" * 105)
    print(f"{'Parameter':<26} | {'Baseline':<8} | {'Tested':<8} | {'Delta Cmds':<11} | {'Delta Troops':<13} | {'Seed Diff':<11} | {'Status':<10}")
    print("-" * 105)

    base_results = {}
    for s in seeds:
        res = run_match(BASELINE, seed=s, max_ticks=args.max_ticks)
        if not res:
            print(f"Failed to run baseline simulation on seed {s}!")
            sys.exit(1)
        base_results[s] = res

    base_total_cmds = sum(r["ai1_cmds"] for r in base_results.values())
    base_total_troops = sum(r["ai1_troops"] for r in base_results.values())

    for param, test_val in PERTURBATIONS.items():
        perturbed = dict(BASELINE)
        perturbed[param] = test_val

        diff_count = 0
        test_total_cmds = 0
        test_total_troops = 0

        for s in seeds:
            test_res = run_match(perturbed, seed=s, max_ticks=args.max_ticks)
            if not test_res:
                continue
            test_total_cmds += test_res["ai1_cmds"]
            test_total_troops += test_res["ai1_troops"]
            
            b = base_results[s]
            if (test_res["ai1_cmds"] != b["ai1_cmds"] or 
                test_res["ai1_troops"] != b["ai1_troops"] or 
                test_res["ticks"] != b["ticks"]):
                diff_count += 1

        d_cmds = test_total_cmds - base_total_cmds
        d_troops = test_total_troops - base_total_troops

        base_str = str(BASELINE[param])
        test_str = str(test_val)
        d_cmds_str = f"{d_cmds:+d}"
        d_troops_str = f"{d_troops:+d}"
        diff_str = f"{diff_count}/{len(seeds)}"
        status = "ACTIVE" if diff_count > 0 else "PASSIVE"

        print(f"{param:<26} | {base_str:<8} | {test_str:<8} | {d_cmds_str:<11} | {d_troops_str:<13} | {diff_str:<11} | {status:<10}")

    print("=" * 105)

if __name__ == "__main__":
    main()
