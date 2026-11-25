#!/usr/bin/env python3
"""
War Strategy - Single Parameter Sensitivity Evaluation on HEAD (M9.12 Item 3)
Đo độ nhạy từng tham số khác biệt trong data/ai_best_params.json so với mặc định.
"""

import subprocess
import json
import os
import re
import sys
import tempfile
from pathlib import Path

DEFAULTS = {
    "decisionIntervalTicks": 20,
    "aggression": 0.75,
    "retreatHpRatio": 0.30,
    "commanderSafeRadiusTiles": 10.0,
    "artilleryStandOffMargin": 2.0,
    "artilleryIdealRangeRatio": 0.80,
    "scoutingWeight": 1.0,
    "focusFireWeight": 1.2,
    "targetMemoryTicks": 140,
    "enableCommanderRetreat": True,
    "enableUnitRetreat": True,
    "scoutingGroupCount": 3,
    "threatRadiusTiles": 16.0,
}

# 3 tham số khác biệt từ data/ai_best_params.json
BEST_PARAMS_DIFFS = {
    "enableUnitRetreat": False,
    "scoutingWeight": 0.3315999507904053,
    "targetMemoryTicks": 161,
}

EXE = Path("build/Release/War_Strategy_Train.exe")

def run_eval(profile_path: Path, seeds: int = 50):
    cmd = [
        str(EXE),
        "--eval-only", str(profile_path),
        "--seeds", str(seeds)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    out = res.stdout
    
    # Parse metrics
    winrate_m = re.search(r"Ty le thang tong \(Mirror\):\s+([\d\.]+)%", out)
    wins_m = re.search(r"Ket qua \(Thang - Thua - Hoa\):\s+(\d+)\s+-\s+(\d+)\s+-\s+(\d+)", out)
    ci_m = re.search(r"Wilson CI 95%:\s+\[([\d\.]+)%,\s+([\d\.]+)%\]", out)
    sign_m = re.search(r"Sign Test \(cap 2-0 vs 0-2\):\s+(\d+)\s+vs\s+(\d+),\s+p-value\s+=\s+([\d\.]+)", out)
    blue_m = re.search(r"Winrate khi Candidate la Blue:\s+([\d\.]+)%", out)
    red_m = re.search(r"Winrate khi Candidate la Red :\s+([\d\.]+)%", out)
    cand_cmd_m = re.search(r"Tu lenh Candidate chet:\s+(\d+)", out)
    base_cmd_m = re.search(r"Tu lenh Baseline chet:\s+(\d+)", out)
    conclusion_m = re.search(r">>> KET LUAN:\s+(.+)", out)

    return {
        "winrate": float(winrate_m.group(1)) if winrate_m else 0.0,
        "wins": int(wins_m.group(1)) if wins_m else 0,
        "losses": int(wins_m.group(2)) if wins_m else 0,
        "draws": int(wins_m.group(3)) if wins_m else 0,
        "ci_lower": float(ci_m.group(1)) if ci_m else 0.0,
        "ci_upper": float(ci_m.group(2)) if ci_m else 0.0,
        "sweep_2_0": int(sign_m.group(1)) if sign_m else 0,
        "sweep_0_2": int(sign_m.group(2)) if sign_m else 0,
        "p_value": float(sign_m.group(3)) if sign_m else 1.0,
        "blue_wr": float(blue_m.group(1)) if blue_m else 0.0,
        "red_wr": float(red_m.group(1)) if red_m else 0.0,
        "cand_cmd_deaths": int(cand_cmd_m.group(1)) if cand_cmd_m else 0,
        "base_cmd_deaths": int(base_cmd_m.group(1)) if base_cmd_m else 0,
        "conclusion": conclusion_m.group(1).strip() if conclusion_m else "N/A"
    }

def main():
    if not EXE.exists():
        print(f"[ERROR] Khong tim thay {EXE}. Hay build Release truoc!", file=sys.stderr)
        sys.exit(1)

    seeds = 50
    if len(sys.argv) > 1:
        seeds = int(sys.argv[1])

    print("=" * 105)
    print(f"WAR STRATEGY - SINGLE-PARAMETER SENSITIVITY BENCHMARK ON HEAD ({seeds} SEEDS = {seeds*2} MATCHES)")
    print("=" * 105)
    
    test_cases = []
    
    # 1. Full ai_best_params.json
    test_cases.append(("Full ai_best_params (All 3)", Path("data/ai_best_params.json"), "Combined: retreat=F, scout=0.33, mem=161"))

    # 2. Single perturbations
    for param_name, custom_val in BEST_PARAMS_DIFFS.items():
        p = dict(DEFAULTS)
        p[param_name] = custom_val
        
        tmp = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
        json.dump({"params": p}, tmp, indent=4)
        tmp.close()
        
        def_str = str(DEFAULTS[param_name])
        custom_str = f"{custom_val:.4f}" if isinstance(custom_val, float) else str(custom_val)
        desc = f"{param_name}: {def_str} -> {custom_str}"
        test_cases.append((param_name, Path(tmp.name), desc))

    results = []
    try:
        for name, path, desc in test_cases:
            print(f"Dang danh gia: {desc} ...", flush=True)
            res = run_eval(path, seeds=seeds)
            results.append((name, desc, res))
    finally:
        # Don file tam
        for name, path, desc in test_cases[1:]:
            if path.exists():
                os.remove(path)

    # In bang tong hop
    print("\n" + "=" * 115)
    print(f"{'Cau Hinh':<26} | {'Winrate':<14} | {'Wilson CI 95%':<15} | {'Sign Test (2-0 vs 0-2)':<23} | {'p-value':<9} | {'Ket Luan':<20}")
    print("-" * 115)
    for name, desc, r in results:
        wr_str = f"{r['winrate']:.1f}% ({r['wins']}/{r['wins']+r['losses']+r['draws']})"
        ci_str = f"[{r['ci_lower']:.1f}%, {r['ci_upper']:.1f}%]"
        sign_str = f"{r['sweep_2_0']:2d} vs {r['sweep_0_2']:2d}"
        pval_str = f"{r['p_value']:.4f}"
        print(f"{name:<26} | {wr_str:<14} | {ci_str:<15} | {sign_str:<23} | {pval_str:<9} | {r['conclusion']:<20}")
    print("=" * 115)

    print("\nChi tiet theo phe va tu lenh chet:")
    print("-" * 100)
    for name, desc, r in results:
        print(f" * {name}: Blue WR={r['blue_wr']:.1f}%, Red WR={r['red_wr']:.1f}%, Cand Cmd Death={r['cand_cmd_deaths']}, Base Cmd Death={r['base_cmd_deaths']}")

if __name__ == "__main__":
    main()
