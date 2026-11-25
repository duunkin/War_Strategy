import subprocess
import re

EXE = "build/Release/War_Strategy_Headless.exe"

def check_seed(seed):
    cmd = [
        EXE,
        "--matches", "1",
        "--seed", str(seed),
        "--max-ticks", "1",
        "--trace-every", "1",
        "--quiet"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = proc.communicate()

    blue_units = {}
    red_units = {}

    for line in stdout.splitlines():
        # Example: 1   AI1_Blue  Tư lệnh  1/1        100 (100%)   (2,48)[2500,48500] ...
        m = re.search(r"(\d+)\s+(AI1_Blue|AI2_Red)\s+(\S+.*?)\s+\d+/\d+\s+\d+\s+\(\d+%\)\s+\((\d+),(\d+)\)", line)
        if m:
            uid = int(m.group(1))
            faction = m.group(2)
            utype = m.group(3).strip()
            x = int(m.group(4))
            y = int(m.group(5))
            if faction == "AI1_Blue":
                blue_units[uid] = (utype, x, y)
            else:
                red_units[uid] = (utype, x, y)

    return blue_units, red_units

def main():
    seeds_to_check = [7919, 1001, 1037, 1074, 1111]
    print(f"=== CHECK SPAWN COORDINATES & SYMMETRY ===")
    
    for s in seeds_to_check:
        b_u, r_u = check_seed(s)
        print(f"\n--- Seed {s} (Blue units: {len(b_u)}, Red units: {len(r_u)}) ---")
        
        # Match units by index: 1..30 vs 31..60
        all_sym = True
        for i in range(1, 31):
            if i in b_u and (i + 30) in r_u:
                b_type, bx, by = b_u[i]
                r_type, rx, ry = r_u[i + 30]
                sum_x = bx + rx
                diff_y = by - ry
                is_sym = (sum_x == 127 and diff_y == 0)
                if not is_sym:
                    all_sym = False
                    print(f"  [ASYM] #{i:02d} {b_type:<15}: Blue=({bx:3d},{by:2d}) | Red=({rx:3d},{ry:2d}) -> x_P+x_E={sum_x}, y_P-y_E={diff_y}")
                else:
                    if i in [1, 2, 12, 14, 16, 20, 22, 27, 29]: # Sample print
                        print(f"  [OK]   #{i:02d} {b_type:<15}: Blue=({bx:3d},{by:2d}) | Red=({rx:3d},{ry:2d}) -> x_P+x_E={sum_x}")
        if all_sym:
            print(f"  => ALL 30 units satisfy x_P + x_E == 127 and y_P == y_E 100% on Seed {s}!")

if __name__ == "__main__":
    main()
