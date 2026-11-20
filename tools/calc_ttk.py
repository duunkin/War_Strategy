"""
================================================================================
tools/calc_ttk.py - Script can bang va tinh toan TTK (Time-To-Kill)
================================================================================
Ghi chu:
  Script nay dung de can bang cac chi so chien dau (dmg, armor, cooldown, weapons)
  giua cac loai don vi trong War Strategy.
  Cong thuc toan hoc trong script nay KHOP 100% voi ma nguon C++ trong
  sim/combat/CombatSystem.cpp va sim/WorldSim.cpp:
    - Fixed-point 1000 voi kieu int64 (khong lam tron tung phat).
    - Noi suy tuyen tinh lien tuc giua cac moc, khong dung nguong bac thang.
    - Tu dong chon vu khi gay tong sat thuong lon nhat len muc tieu.
    - Sat thuong no lan (splash) tinh doc lap theo giap va loai cua tung cham dich bi trung (55%).
================================================================================
"""

units = {
    'COMMANDER': {
        'hpPerUnit': 100, 'unitsPerDot': 1, 'armor': 0, 'cooldownTicks': 24, 'domain': 'LAND',
        'weapons': [{'ammo': 'AP', 'dmg': 50, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}]
    },
    'INFANTRY': {
        'hpPerUnit': 100, 'unitsPerDot': 3000, 'armor': 8, 'cooldownTicks': 20, 'domain': 'LAND',
        'weapons': [{'ammo': 'AP', 'dmg': 3, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': True}]
    },
    'LEOPARD_2A': {
        'hpPerUnit': 2000, 'unitsPerDot': 40, 'armor': 68, 'cooldownTicks': 32, 'domain': 'LAND',
        'weapons': [
            {'ammo': 'AP', 'dmg': 200, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 200, 'splash': 1.0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    },
    'T14_ARMATA': {
        'hpPerUnit': 1750, 'unitsPerDot': 50, 'armor': 54, 'cooldownTicks': 28, 'domain': 'LAND',
        'weapons': [
            {'ammo': 'AP', 'dmg': 150, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 150, 'splash': 1.0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    },
    'PUMA': {
        'hpPerUnit': 1250, 'unitsPerDot': 50, 'armor': 26, 'cooldownTicks': 16, 'domain': 'LAND',
        'weapons': [
            {'ammo': 'AP', 'dmg': 100, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 100, 'splash': 1.0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    },
    'CAESAR': {
        'hpPerUnit': 1000, 'unitsPerDot': 25, 'armor': 12, 'cooldownTicks': 50, 'domain': 'LAND',
        'weapons': [{'ammo': 'HE', 'dmg': 200, 'splash': 1.5, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}]
    },
    'GEPARD': {
        'hpPerUnit': 750, 'unitsPerDot': 25, 'armor': 28, 'cooldownTicks': 40, 'domain': 'LAND',
        'weapons': [{'ammo': 'AP', 'dmg': 20, 'splash': 0, 'rounds': 50, 'accuracy': 0.2, 'smallArms': False}]
    },
    'SU57': {
        'hpPerUnit': 500, 'unitsPerDot': 25, 'armor': 16, 'cooldownTicks': 60, 'domain': 'AIR',
        'weapons': [{'ammo': 'AP', 'dmg': 200, 'splash': 0, 'rounds': 1, 'accuracy': 0.6, 'smallArms': False}]
    },
    'B2_SPIRIT': {
        'hpPerUnit': 1000, 'unitsPerDot': 10, 'armor': 14, 'cooldownTicks': 55, 'domain': 'AIR',
        'weapons': [
            {'ammo': 'AP', 'dmg': 400, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 400, 'splash': 2.0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    },
    'FFM3_MOGAMI': {
        'hpPerUnit': 4000, 'unitsPerDot': 10, 'armor': 42, 'cooldownTicks': 26, 'domain': 'SEA',
        'weapons': [
            {'ammo': 'AP', 'dmg': 300, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 300, 'splash': 1.5, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    },
    'DD151_ASAGIRI': {
        'hpPerUnit': 3500, 'unitsPerDot': 25, 'armor': 55, 'cooldownTicks': 34, 'domain': 'SEA',
        'weapons': [
            {'ammo': 'AP', 'dmg': 150, 'splash': 0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False},
            {'ammo': 'HE', 'dmg': 150, 'splash': 1.0, 'rounds': 1, 'accuracy': 1.0, 'smallArms': False}
        ]
    }
}

def get_he_bonus(target_armor):
    if target_armor <= 0:
        return 1450
    elif target_armor >= 20:
        return 1000
    else:
        return 1450 - (target_armor * 450) // 20

def get_he_armor_factor(target_armor):
    reduction = min(840, target_armor * 14)
    return 1000 - reduction

def get_ap_bonus(target_armor, is_small_arms):
    if is_small_arms:
        return 1000
    if target_armor <= 0:
        return 1000
    elif target_armor >= 30:
        return 1200
    else:
        return 1000 + (target_armor * 200) // 30

def get_small_arms_factor(target_armor):
    if target_armor <= 10:
        return 1000
    elif target_armor <= 20:
        return 1000 - ((target_armor - 10) * 500) // 10
    elif target_armor <= 30:
        return 500 - ((target_armor - 20) * 300) // 10
    else:
        return 200

def get_ap_armor_factor(target_armor):
    reduction = min(720, target_armor * 8)
    return 1000 - reduction

def get_anti_air_factor(attacker_type, target_domain):
    if target_domain == 'AIR' and attacker_type in ('GEPARD', 'SU57'):
        return 1500
    return 1000

def calc_weapon_damage(attacker_type, w, n_alive, target_type, is_splash=False):
    t_cfg = units[target_type]
    t_armor = t_cfg['armor']
    t_domain = t_cfg['domain']

    rounds = w.get('rounds', 1)
    acc = w.get('accuracy', 1.0)
    hits = int(round(n_alive * rounds * acc))
    if hits <= 0:
        return 0

    dmg = w['dmg']

    if w['ammo'] == 'HE':
        c_ammo = get_he_bonus(t_armor)
        c_armor = get_he_armor_factor(t_armor)
    else:
        is_sa = w.get('smallArms', False)
        c_ammo = get_ap_bonus(t_armor, is_sa)
        if is_sa:
            c_ammo = (c_ammo * get_small_arms_factor(t_armor)) // 1000
        c_armor = get_ap_armor_factor(t_armor)

    c_air = get_anti_air_factor(attacker_type, t_domain)
    c_splash = 550 if is_splash else 1000

    num = hits * dmg * c_ammo * c_armor * c_air * c_splash
    den = 1000 * 1000 * 1000 * 1000
    total_dmg = num // den
    if total_dmg < 1 and hits > 0:
        total_dmg = 1
    return total_dmg

def choose_best_weapon(attacker_type, n_alive, target_type):
    a_cfg = units[attacker_type]
    best_w = None
    best_dmg = -1
    for w in a_cfg['weapons']:
        dmg = calc_weapon_damage(attacker_type, w, n_alive, target_type)
        if dmg > best_dmg:
            best_dmg = dmg
            best_w = w
    return best_w, best_dmg

def main():
    print("=" * 80)
    print("BANG SAT THUONG VA THOI GIAN TIEU DIET (TTK)")
    print("=" * 80)

    pairs = [
        ('INFANTRY', 'INFANTRY'),
        ('LEOPARD_2A', 'INFANTRY'),
        ('INFANTRY', 'LEOPARD_2A'),
        ('T14_ARMATA', 'LEOPARD_2A'),
        ('CAESAR', 'INFANTRY'),
        ('SU57', 'LEOPARD_2A'),
        ('FFM3_MOGAMI', 'DD151_ASAGIRI'),
        ('DD151_ASAGIRI', 'FFM3_MOGAMI'),
        ('LEOPARD_2A', 'T14_ARMATA')
    ]

    for a, b in pairs:
        w, dmg = choose_best_weapon(a, units[a]['unitsPerDot'], b)
        ammo = w['ammo']
        cd_sec = units[a]['cooldownTicks'] / 20.0
        target_hp = units[b]['hpPerUnit'] * units[b]['unitsPerDot']
        dps = dmg / cd_sec
        dummy_ttk = target_hp / dps
        print(f"{a:14} -> {b:14} | weapon={ammo:2} | dmg/volley={dmg:6} | cd={cd_sec:4.1f}s | TargetHP={target_hp:6} | DummyTTK={dummy_ttk:5.1f}s")

if __name__ == '__main__':
    main()
