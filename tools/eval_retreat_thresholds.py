#!/usr/bin/env python3
"""
War Strategy - Evaluation of Unit Retreat Thresholds (M9.13)
Kiem tra nguong rut lui:
1) 6 profile vs AiParams mac dinh (50 seeds)
2) 6 profile vs data/ai_best_params.json (50 seeds)
3) data/ai_best_params.json vs Mac dinh voi --eval-flip-map (50 seeds)
"""

import subprocess
import json
import os
import re
import sys
import tempfile
from pathlib import Path
from typing import Dict, Any, List, Optional

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

PROFILES = [
    {
        "name": "Retreat OFF",
        "desc": "enableUnitRetreat=false",
        "overrides": {"enableUnitRetreat": False, "retreatHpRatio": 0.30}
    },
    {
        "name": "Retreat ON (5%)",
        "desc": "enableUnitRetreat=true, retreatHpRatio=0.05",
        "overrides": {"enableUnitRetreat": True, "retreatHpRatio": 0.05}
    },
    {
        "name": "Retreat ON (10%)",
        "desc": "enableUnitRetreat=true, retreatHpRatio=0.10",
        "overrides": {"enableUnitRetreat": True, "retreatHpRatio": 0.10}
    },
    {
        "name": "Retreat ON (15%)",
        "desc": "enableUnitRetreat=true, retreatHpRatio=0.15",
        "overrides": {"enableUnitRetreat": True, "retreatHpRatio": 0.15}
    },
    {
        "name": "Retreat ON (20%)",
        "desc": "enableUnitRetreat=true, retreatHpRatio=0.20",
        "overrides": {"enableUnitRetreat": True, "retreatHpRatio": 0.20}
    },
    {
        "name": "Retreat ON (30% - Def)",
        "desc": "enableUnitRetreat=true, retreatHpRatio=0.30 (Mac dinh)",
        "overrides": {"enableUnitRetreat": True, "retreatHpRatio": 0.30}
    },
]

EXE = Path("build/Release/War_Strategy_Train.exe")

def parse_stats_section(text: str) -> Dict[str, Any]:
    total_m = re.search(r"Tong so tran:\s+(\d+)\s+\((\d+)\s+cap seed\)", text)
    res_m = re.search(r"Ket qua \(Thang - Thua - Hoa\):\s+(\d+)\s+-\s+(\d+)\s+-\s+(\d+)", text)
    wr_m = re.search(r"Ty le thang tong:\s+([\d\.]+)%", text)
    blue_wr_m = re.search(r"Winrate khi Candidate la Blue:\s+([\d\.]+)%\s+\((\d+)/(\d+)\)", text)
    red_wr_m = re.search(r"Winrate khi Candidate la Red :\s+([\d\.]+)%\s+\((\d+)/(\d+)\)", text)
    ci_m = re.search(r"Wilson CI 95%:\s+\[([\d\.]+)%,\s+([\d\.]+)%\]", text)
    sign_m = re.search(r"Sign Test \(cap 2-0 vs 0-2\):\s+(\d+)\s+vs\s+(\d+),\s+p-value\s+=\s+([\d\.]+)", text)
    cand_cmd_m = re.search(r"Tu lenh Candidate chet:\s+(\d+)\s+/\s+(\d+)\s+tran", text)
    base_cmd_m = re.search(r"Tu lenh Baseline chet:\s+(\d+)\s+/\s+(\d+)\s+tran", text)
    timeout_m = re.search(r"Tran ket thuc do het gio:\s+(\d+)\s+/\s+(\d+)\s+tran", text)
    conclusion_m = re.search(r">>> KET LUAN:\s+(.+)", text)
    sweep_m = re.search(r"Cap 2-0 \(Candidate sweep thang ca 2 tran\):\s+(\d+)", text)
    sweep02_m = re.search(r"Cap 0-2 \(Baseline sweep thang ca 2 tran\):\s+(\d+)", text)
    split11_m = re.search(r"Cap 1-1 \(Hai tran cung thuoc ve 1 phe\):\s+(\d+)", text)

    return {
        "total_matches": int(total_m.group(1)) if total_m else 0,
        "seed_pairs": int(total_m.group(2)) if total_m else 0,
        "wins": int(res_m.group(1)) if res_m else 0,
        "losses": int(res_m.group(2)) if res_m else 0,
        "draws": int(res_m.group(3)) if res_m else 0,
        "winrate": float(wr_m.group(1)) if wr_m else 0.0,
        "blue_wr": float(blue_wr_m.group(1)) if blue_wr_m else 0.0,
        "red_wr": float(red_wr_m.group(1)) if red_wr_m else 0.0,
        "ci_lower": float(ci_m.group(1)) if ci_m else 0.0,
        "ci_upper": float(ci_m.group(2)) if ci_m else 0.0,
        "sweep_2_0": int(sign_m.group(1)) if sign_m else 0,
        "sweep_0_2": int(sign_m.group(2)) if sign_m else 0,
        "p_value": float(sign_m.group(3)) if sign_m else 1.0,
        "split_1_1": int(split11_m.group(1)) if split11_m else 0,
        "cand_cmd_deaths": int(cand_cmd_m.group(1)) if cand_cmd_m else 0,
        "base_cmd_deaths": int(base_cmd_m.group(1)) if base_cmd_m else 0,
        "timeout_matches": int(timeout_m.group(1)) if timeout_m else 0,
        "conclusion": conclusion_m.group(1).strip() if conclusion_m else "N/A"
    }

def run_eval_command(profile_path: Path, baseline_path: Optional[Path] = None,
                     seeds: int = 50, flip_map: bool = False) -> str:
    cmd = [
        str(EXE),
        "--eval-only", str(profile_path),
        "--seeds", str(seeds)
    ]
    if baseline_path is not None:
        cmd.extend(["--baseline", str(baseline_path)])
    if flip_map:
        cmd.append("--eval-flip-map")

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Command failed with exit code {res.returncode}:\n{res.stderr}", file=sys.stderr)
    return res.stdout

def create_temp_profile(overrides: Dict[str, Any]) -> str:
    params = dict(DEFAULTS)
    params.update(overrides)
    tmp = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
    json.dump({"params": params}, tmp, indent=4)
    tmp.close()
    return tmp.name

def print_table_header():
    print(f"| {'Profile':<24} | {'Winrate (W-L-D)':<20} | {'Wilson CI 95%':<16} | {'Sign Test (2-0 vs 0-2)':<23} | {'p-value':<9} | {'Tu Lenh Chet (Cand/Base)':<25} | {'Het Gio':<10} |")
    print(f"|{'-'*26}|{'-'*22}|{'-'*18}|{'-'*25}|{'-'*11}|{'-'*27}|{'-'*12}|")

def print_table_row(name: str, s: Dict[str, Any]):
    wr_str = f"{s['winrate']:.1f}% ({s['wins']}-{s['losses']}-{s['draws']})"
    ci_str = f"[{s['ci_lower']:.1f}%, {s['ci_upper']:.1f}%]"
    sign_str = f"{s['sweep_2_0']:2d} vs {s['sweep_0_2']:2d}"
    pval_str = f"{s['p_value']:.4f}"
    cmd_str = f"{s['cand_cmd_deaths']:2d} / {s['base_cmd_deaths']:2d}"
    to_str = f"{s['timeout_matches']:2d}/{s['total_matches']}"
    print(f"| {name:<24} | {wr_str:<20} | {ci_str:<16} | {sign_str:<23} | {pval_str:<9} | {cmd_str:<25} | {to_str:<10} |")

def run_part1(seeds: int = 50):
    print("\n" + "=" * 110)
    print(f"MUC 1: KIEM TRA 6 PROFILE VS DOI THU MAC DINH ({seeds} SEEDS = {seeds*2} TRAN)")
    print("=" * 110, flush=True)
    print_table_header()

    results = []
    for idx, prof in enumerate(PROFILES, 1):
        tmp_path = create_temp_profile(prof["overrides"])
        try:
            print(f"[{idx}/{len(PROFILES)}] Dang chay {prof['name']} ...", file=sys.stderr, flush=True)
            out = run_eval_command(Path(tmp_path), baseline_path=None, seeds=seeds)
            stats = parse_stats_section(out)
            results.append({"profile": prof, "stats": stats})
            print_table_row(prof["name"], stats)
            sys.stdout.flush()
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)
    return results

def run_part2(seeds: int = 50):
    baseline = Path("data/ai_best_params.json")
    print("\n" + "=" * 110)
    print(f"MUC 2: KIEM TRA 6 PROFILE VS DOI THU ai_best_params.json ({seeds} SEEDS = {seeds*2} TRAN)")
    print("=" * 110, flush=True)
    print_table_header()

    results = []
    for idx, prof in enumerate(PROFILES, 1):
        tmp_path = create_temp_profile(prof["overrides"])
        try:
            print(f"[{idx}/{len(PROFILES)}] Dang chay {prof['name']} ...", file=sys.stderr, flush=True)
            out = run_eval_command(Path(tmp_path), baseline_path=baseline, seeds=seeds)
            stats = parse_stats_section(out)
            results.append({"profile": prof, "stats": stats})
            print_table_row(prof["name"], stats)
            sys.stdout.flush()
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)
    return results

def run_part3(seeds: int = 50):
    profile = Path("data/ai_best_params.json")
    print("\n" + "=" * 110)
    print(f"MUC 3: data/ai_best_params.json VS MAC DINH VOI --eval-flip-map ({seeds} SEEDS)")
    print("=" * 110, flush=True)

    print("[1/1] Dang chay ai_best_params.json voi --eval-flip-map ...", file=sys.stderr, flush=True)
    out = run_eval_command(profile, baseline_path=None, seeds=seeds, flip_map=True)

    # Cat 3 section
    sections = re.split(r"={5,}\s*\n\s*KET QUA", out)
    map_results = {}
    titles = {
        "Original Map": r"BAN DO GOC",
        "Flipped Map": r"BAN DO LAT GUONG",
        "Combined": r"COMBINED"
    }

    for key, pattern in titles.items():
        found = False
        for sec in sections[1:]:
            if re.search(pattern, sec):
                map_results[key] = parse_stats_section(sec)
                found = True
                break
        if not found:
            print(f"[WARN] Khong tim thay section {key}", file=sys.stderr)

    print_table_header()
    for name, s in map_results.items():
        print_table_row(name, s)
    sys.stdout.flush()

    return map_results

def main():
    if not EXE.exists():
        print(f"[ERROR] Khong tim thay {EXE}. Build truoc!", file=sys.stderr)
        sys.exit(1)

    part = "all"
    seeds = 50
    out_json = "data/eval_retreat_m9_13.json"
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--part" and i + 1 < len(args):
            part = args[i+1]
            i += 2
        elif args[i] == "--seeds" and i + 1 < len(args):
            seeds = int(args[i+1])
            i += 2
        elif args[i] == "--out" and i + 1 < len(args):
            out_json = args[i+1]
            i += 2
        else:
            i += 1

    all_data = {}
    if os.path.exists(out_json):
        try:
            with open(out_json, "r", encoding="utf-8") as f:
                all_data = json.load(f)
        except Exception:
            all_data = {}

    if part in ("1", "all"):
        r1 = run_part1(seeds)
        all_data["part1_vs_default"] = r1
    if part in ("2", "all"):
        r2 = run_part2(seeds)
        all_data["part2_vs_best_params"] = r2
    if part in ("3", "all"):
        r3 = run_part3(seeds)
        all_data["part3_flip_map"] = r3

    os.makedirs(os.path.dirname(out_json), exist_ok=True)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(all_data, f, indent=4, ensure_ascii=False)
    print(f"\n[OK] Da luu toan bo du lieu chi tiet vao: {out_json}")

if __name__ == "__main__":
    main()
