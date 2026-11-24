#!/usr/bin/env python3
"""
War Strategy - Profile Parameters Diff Tool (M9.12 Item 2)
So sánh bộ tham số trong file profile JSON với bộ tham số AiParams mặc định.
"""

import json
import sys
import argparse
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

def compare_profile(json_path: Path):
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    
    params = data.get("params", data)
    metadata = data.get("metadata", {})
    
    diffs = []
    all_rows = []
    
    for key, def_val in DEFAULTS.items():
        val = params.get(key, def_val)
        
        is_diff = False
        if isinstance(def_val, float):
            is_diff = abs(float(val) - float(def_val)) >= 1e-4
        else:
            is_diff = (val != def_val)
            
        all_rows.append((key, def_val, val, is_diff))
        if is_diff:
            diffs.append((key, def_val, val))
            
    return metadata, diffs, all_rows

def main():
    parser = argparse.ArgumentParser(description="Compare AiParams profile with default AiParams")
    parser.add_argument("profile", nargs="?", default="data/ai_best_params.json", help="Path to profile JSON")
    parser.add_argument("--all", action="store_true", help="Print all parameters including identical ones")
    args = parser.parse_args()
    
    profile_path = Path(args.profile)
    if not profile_path.exists():
        print(f"[ERROR] Khong tim thay file profile: {profile_path}", file=sys.stderr)
        sys.exit(1)
        
    meta, diffs, all_rows = compare_profile(profile_path)
    
    print("=" * 78)
    print(f"SO SANH THAM SO: {profile_path}")
    if meta:
        print(f"Metadata: gen={meta.get('generation')}, fit={meta.get('fitness')}, "
              f"winrateVsBase={meta.get('winRateVsBaseline')}, algo={meta.get('algorithm')}")
    print("=" * 78)
    print(f"{'STT':<4} | {'Ten Tham So':<26} | {'Mac Dinh':<12} | {'Gia Tri Profile':<15} | {'Trang Thai':<10}")
    print("-" * 78)
    
    rows_to_print = all_rows if args.all else [r for r in all_rows if r[3]]
    
    if not rows_to_print:
        print("  (Khong co tham so nao khac mac dinh - profile 100% giong AiParams{})")
    else:
        for idx, (key, def_val, val, is_diff) in enumerate(rows_to_print, 1):
            status = "KHAC BIET" if is_diff else "Giong"
            print(f"{idx:<4} | {key:<26} | {str(def_val):<12} | {str(val):<15} | {status:<10}")
            
    print("=" * 78)
    print(f"Tong so tham so khac biet: {len(diffs)} / {len(DEFAULTS)}\n")
    
    if diffs:
        print("Danh sach tham so tao ra chenh lech:")
        for idx, (key, def_val, val) in enumerate(diffs, 1):
            print(f"  {idx}. {key}: mac dinh = {def_val} -> profile = {val}")

if __name__ == "__main__":
    main()
