#include "CombatSystem.hpp"
#include <algorithm>
#include <cmath>

namespace sim {

int64_t CombatSystem::getHeBonus(int targetArmor) {
    if (targetArmor <= 0) return 1450;
    if (targetArmor >= 20) return 1000;
    return 1450 - (static_cast<int64_t>(targetArmor) * 450) / 20;
}

int64_t CombatSystem::getHeArmorFactor(int targetArmor) {
    int64_t reduction = std::min<int64_t>(840, static_cast<int64_t>(targetArmor) * 14);
    return 1000 - reduction;
}

int64_t CombatSystem::getApBonus(int targetArmor, bool isSmallArms) {
    if (isSmallArms) return 1000;
    if (targetArmor <= 0) return 1000;
    if (targetArmor >= 30) return 1200;
    return 1000 + (static_cast<int64_t>(targetArmor) * 200) / 30;
}

int64_t CombatSystem::getSmallArmsFactor(int targetArmor) {
    if (targetArmor <= 10) return 1000;
    if (targetArmor <= 20) {
        return 1000 - (static_cast<int64_t>(targetArmor - 10) * 500) / 10;
    }
    if (targetArmor <= 30) {
        return 500 - (static_cast<int64_t>(targetArmor - 20) * 300) / 10;
    }
    return 200;
}

int64_t CombatSystem::getApArmorFactor(int targetArmor) {
    int64_t reduction = std::min<int64_t>(720, static_cast<int64_t>(targetArmor) * 8);
    return 1000 - reduction;
}

int64_t CombatSystem::getAntiAirFactor(UnitType attackerType, UnitDomain targetDomain) {
    if (targetDomain == UnitDomain::Air && (attackerType == UnitType::Gepard || attackerType == UnitType::Su57)) {
        return 1500;
    }
    return 1000;
}

int64_t CombatSystem::calculateVolleyDamage(
    UnitType attackerType,
    const WeaponConfig& weapon,
    int aliveUnits,
    int targetArmor,
    UnitDomain targetDomain,
    bool isSplash,
    int hitsOverride,
    int64_t splashRatio
) {
    if (aliveUnits <= 0) return 0;

    int64_t hits = 0;
    if (hitsOverride >= 0) {
        hits = hitsOverride;
    } else {
        double rawHits = static_cast<double>(aliveUnits) * weapon.rounds * weapon.accuracy;
        hits = static_cast<int64_t>(std::round(rawHits));
    }
    if (hits <= 0) return 0;

    int64_t dmg = weapon.dmg;
    int64_t cAmmo = 1000;
    int64_t cArmor = 1000;

    if (weapon.ammo == "HE") {
        cAmmo = getHeBonus(targetArmor);
        cArmor = getHeArmorFactor(targetArmor);
    } else {
        bool isSa = weapon.smallArms;
        cAmmo = getApBonus(targetArmor, isSa);
        if (isSa) {
            cAmmo = (cAmmo * getSmallArmsFactor(targetArmor)) / 1000;
        }
        cArmor = getApArmorFactor(targetArmor);
    }

    int64_t cAir = getAntiAirFactor(attackerType, targetDomain);
    int64_t cSplash = isSplash ? splashRatio : 1000;

    int64_t num = hits * dmg * cAmmo * cArmor * cAir * cSplash;
    int64_t den = 1000LL * 1000LL * 1000LL * 1000LL;
    int64_t totalDamage = num / den;

    if (totalDamage < 1 && hits > 0) {
        totalDamage = 1;
    }
    return totalDamage;
}

size_t CombatSystem::chooseBestWeaponIndex(
    UnitType attackerType,
    const std::vector<WeaponConfig>& weapons,
    int aliveUnits,
    int targetArmor,
    UnitDomain targetDomain
) {
    if (weapons.empty()) return 0;
    if (weapons.size() == 1) return 0;

    size_t bestIdx = 0;
    int64_t bestDmg = -1;

    for (size_t i = 0; i < weapons.size(); ++i) {
        int64_t dmg = calculateVolleyDamage(attackerType, weapons[i], aliveUnits, targetArmor, targetDomain, false);
        if (dmg > bestDmg) {
            bestDmg = dmg;
            bestIdx = i;
        }
    }
    return bestIdx;
}

bool CombatSystem::canAttackTarget(const Unit& attacker, const Unit& target) {
    if (attacker.isDead() || target.isDead()) return false;
    if (attacker.getFaction() == target.getFaction()) return false;
    if (attacker.getFaction() == Faction::Neutral || target.getFaction() == Faction::Neutral) return false;

    // Quy tắc: chỉ tàu bắn được tàu, các loại khác không thể tấn công tàu
    if (target.getDomain() == UnitDomain::Sea) {
        if (attacker.getDomain() != UnitDomain::Sea) {
            return false;
        }
    }

    // Tàu chỉ bắn được tàu và mục tiêu trên bờ, không bắn máy bay
    if (target.getDomain() == UnitDomain::Air) {
        if (!attacker.canTargetAir()) return false;
    } else if (target.getDomain() == UnitDomain::Land) {
        if (!attacker.canTargetGround()) return false;
    }

    // Khoảng cách trong tầm bắn (mili-ô)
    int64_t dx = static_cast<int64_t>(target.getPosX()) - attacker.getPosX();
    int64_t dy = static_cast<int64_t>(target.getPosY()) - attacker.getPosY();
    int64_t distSqMilli = dx * dx + dy * dy;

    int64_t rangeMilli = static_cast<int64_t>(attacker.getRangeTiles() * 1000.0f);
    int64_t rangeSqMilli = rangeMilli * rangeMilli;

    return distSqMilli <= rangeSqMilli;
}

} // namespace sim
