import subprocess
import re
import sys
import json
from pathlib import Path

EXE = "build/Release/War_Strategy_Train.exe"
PROFILE = "data/ai_best_params.json"

def run_eval(seed_base, seeds=100):
    cmd = [
        EXE,
        "--eval-only", PROFILE,
        "--vs-baseline",
        "--seeds", str(seeds),
        "--seed-base", str(seed_base),
        "--quiet"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()
    return stdout

def parse_eval_output(output):
    res = {}
    m_meta = re.search(r"\[Profile Metadata\]\s*(.*)", output)
    if m_meta:
        res["metadata"] = m_meta.group(1).strip()
    
    m_mirrors = re.search(r"Tong so tran mirror:\s*(\d+)\s*\((\d+)\s*seeds\)", output)
    if m_mirrors:
        res["matches"] = int(m_mirrors.group(1))
        res["seeds"] = int(m_mirrors.group(2))
        
    m_rec = re.search(r"Ket qua \(Thang - Thua - Hoa\):\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)", output)
    if m_rec:
        res["wins"] = int(m_rec.group(1))
        res["losses"] = int(m_rec.group(2))
        res["draws"] = int(m_rec.group(3))
        
    m_wr = re.search(r"Ty le thang tong \(Mirror\):\s*([\d\.]+)%", output)
    if m_wr:
        res["winrate"] = float(m_wr.group(1))
        
    m_blue = re.search(r"Winrate khi Candidate la Blue:\s*([\d\.]+)%\s*\((\d+)/(\d+)\)", output)
    if m_blue:
        res["wr_blue"] = float(m_blue.group(1))
        res["w_blue"] = int(m_blue.group(2))
        
    m_red = re.search(r"Winrate khi Candidate la Red\s*:\s*([\d\.]+)%\s*\((\d+)/(\d+)\)", output)
    if m_red:
        res["wr_red"] = float(m_red.group(1))
        res["w_red"] = int(m_red.group(2))
        
    m_20 = re.search(r"Cap 2-0.*?:\s*(\d+)\s*\(([\d\.]+)%\)", output)
    if m_20:
        res["pairs_2_0"] = int(m_20.group(1))
        res["pct_2_0"] = float(m_20.group(2))
        
    m_02 = re.search(r"Cap 0-2.*?:\s*(\d+)\s*\(([\d\.]+)%\)", output)
    if m_02:
        res["pairs_0_2"] = int(m_02.group(1))
        res["pct_0_2"] = float(m_02.group(2))
        
    m_11 = re.search(r"Cap 1-1.*?:\s*(\d+)\s*\(([\d\.]+)%\)", output)
    if m_11:
        res["pairs_1_1"] = int(m_11.group(1))
        res["pct_1_1"] = float(m_11.group(2))
        
    m_ci = re.search(r"Wilson CI 95%:\s*\[([\d\.]+)%,\s*([\d\.]+)%\]", output)
    if m_ci:
        res["ci_low"] = float(m_ci.group(1))
        res["ci_high"] = float(m_ci.group(2))
        
    m_sign = re.search(r"Sign Test \(cap 2-0 vs 0-2\):\s*(\d+)\s*vs\s*(\d+),\s*p-value\s*=\s*([\d\.]+)", output)
    if m_sign:
        res["sign_w"] = int(m_sign.group(1))
        res["sign_l"] = int(m_sign.group(2))
        res["p_value"] = float(m_sign.group(3))
        
    m_concl = re.search(r">>> KET LUAN:\s*(.*)", output)
    if m_concl:
        res["conclusion"] = m_concl.group(1).strip()
        
    return res

def main():
    if not Path(PROFILE).exists():
        print(f"Error: Profile {PROFILE} not found!")
        sys.exit(1)
        
    with open(PROFILE, "r", encoding="utf-8") as f:
        pdata = json.load(f)
    meta = pdata.get("metadata", {})
    print("==========================================================================")
    print("M9.11 ITEM 2: EVALUATION OF ai_best_params.json VS BASELINE (DUAL RANGES)")
    print("==========================================================================")
    print(f"Profile: {PROFILE}")
    print(f"Metadata in JSON:")
    print(f"  - generation: {meta.get('generation', 'N/A')}")
    print(f"  - fitness: {meta.get('fitness', 'N/A')}")
    print(f"  - winRateVsBaseline: {meta.get('winRateVsBaseline', 'N/A')}")
    print(f"  - algorithm: {meta.get('algorithm', 'N/A')}")
    print(f"  - timestamp: {meta.get('timestamp', 'N/A')}")
    print("--------------------------------------------------------------------------")
    
    for base in [5000, 9000]:
        print(f"\nRunning 100 seeds with seed-base = {base} (200 mirror matches)...")
        out = run_eval(base, seeds=100)
        parsed = parse_eval_output(out)
        print(f"Results for seed-base {base}:")
        print(f"  - Winrate Total: {parsed.get('winrate')}% ({parsed.get('wins')}-{parsed.get('losses')}-{parsed.get('draws')})")
        print(f"  - Candidate as Blue: {parsed.get('wr_blue')}% ({parsed.get('w_blue')}/100)")
        print(f"  - Candidate as Red : {parsed.get('wr_red')}% ({parsed.get('w_red')}/100)")
        print(f"  - Pairs Breakdown: 2-0: {parsed.get('pairs_2_0')} ({parsed.get('pct_2_0')}%), 0-2: {parsed.get('pairs_0_2')} ({parsed.get('pct_0_2')}%), 1-1: {parsed.get('pairs_1_1')} ({parsed.get('pct_1_1')}%)")
        print(f"  - Wilson CI 95%: [{parsed.get('ci_low')}%, {parsed.get('ci_high')}%]")
        print(f"  - Paired Sign Test: {parsed.get('sign_w')} vs {parsed.get('sign_l')}, p-value = {parsed.get('p_value'):.4f}")
        print(f"  - Conclusion: {parsed.get('conclusion')}")

if __name__ == "__main__":
    main()
