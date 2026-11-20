#include "sim/units/Unit.hpp"
#include <cstdlib>

namespace sim {

// Thuật toán căn bậc hai số nguyên 64-bit Newton-Raphson (thuần số nguyên, không dùng float)
static uint64_t integerSqrt64(uint64_t n) {
    if (n <= 1) return n;
    uint64_t x0 = n / 2;
    uint64_t x1 = (x0 + n / x0) / 2;
    while (x1 < x0) {
        x0 = x1;
        x1 = (x0 + n / x0) / 2;
    }
    return x0;
}

Unit::Unit(uint32_t id, Faction faction, UnitType type, int32_t posX, int32_t posY, const UnitTypeConfig& config)
    : m_id(id)
    , m_faction(faction)
    , m_type(type)
    , m_domain(config.domain)
    , m_name(config.name)
    , m_posX(posX)
    , m_posY(posY)
    , m_hpPerUnit(config.hpPerUnit > 0 ? config.hpPerUnit : 100)
    , m_maxUnits(config.unitsPerDot > 0 ? config.unitsPerDot : 1)
    , m_currentUnits(m_maxUnits)
    , m_hp(m_maxUnits * m_hpPerUnit)
    , m_maxHp(m_hp)
    , m_armor(config.armor)
    , m_speedTilesPerSec(config.speedTilesPerSec)
    , m_speedMilliPerSec(config.speedMilliPerSec)
    , m_rangeTiles(config.rangeTiles)
    , m_visionTiles(config.visionTiles)
    , m_cooldownTicks(config.cooldownTicks)
    , m_weapons(config.weapons)
    , m_canTargetAir(config.canTargetAir)
    , m_canTargetGround(config.canTargetGround)
    , m_canTargetSea(config.canTargetSea)
    , m_stealthRevealRangeTiles(config.stealthRevealRangeTiles)
    , m_iconSizeTiles(config.iconSizeTiles)
{}

void Unit::takeDamage(int damage) {
    if (damage <= 0) return;
    if (damage >= m_hp) {
        m_hp = 0;
        m_currentUnits = 0;
    } else {
        m_hp -= damage;
        // Quy tắc: N_alive = ceil(hp / hpPerUnit) = (hp + hpPerUnit - 1) / hpPerUnit
        m_currentUnits = (m_hp + m_hpPerUnit - 1) / m_hpPerUnit;
    }
}

void Unit::setPositionMilli(int32_t x, int32_t y) {
    m_posX = x;
    m_posY = y;
}

void Unit::setDestinationTile(int tileX, int tileY) {
    // Đặt đích đến là tâm của ô vuông (tile * 1000 + 500)
    setDestinationMilli(tileX * 1000 + 500, tileY * 1000 + 500);
}

void Unit::setDestinationMilli(int32_t destX, int32_t destY) {
    m_destX = destX;
    m_destY = destY;
    m_hasDestination = true;
}

void Unit::clearDestination() {
    m_hasDestination = false;
    m_moveAccumulator = 0;
}

void Unit::stepTowardsDestination(int stepMilli) {
    if (!m_hasDestination || stepMilli <= 0) {
        return;
    }

    int64_t dx = static_cast<int64_t>(m_destX) - m_posX;
    int64_t dy = static_cast<int64_t>(m_destY) - m_posY;

    // Khoảng cách Euclid bình phương
    uint64_t distSq = static_cast<uint64_t>(dx * dx + dy * dy);
    if (distSq == 0) {
        m_hasDestination = false;
        return;
    }

    uint64_t dist = integerSqrt64(distSq);

    // Nếu khoảng cách còn lại nhỏ hơn hoặc bằng bước đi, đơn vị đến đích
    if (dist <= static_cast<uint64_t>(stepMilli)) {
        m_posX = m_destX;
        m_posY = m_destY;
        m_hasDestination = false;
        return;
    }

    // Di chuyển tịnh tiến theo vector (dx, dy) với độ dài stepMilli (làm tròn số nguyên)
    int64_t stepX = (dx * stepMilli + (dx >= 0 ? static_cast<int64_t>(dist / 2) : -static_cast<int64_t>(dist / 2))) / static_cast<int64_t>(dist);
    int64_t stepY = (dy * stepMilli + (dy >= 0 ? static_cast<int64_t>(dist / 2) : -static_cast<int64_t>(dist / 2))) / static_cast<int64_t>(dist);

    if (stepX == 0 && stepY == 0) {
        if (std::abs(dx) >= std::abs(dy)) {
            stepX = (dx > 0) ? 1 : -1;
        } else {
            stepY = (dy > 0) ? 1 : -1;
        }
    }

    m_posX += static_cast<int32_t>(stepX);
    m_posY += static_cast<int32_t>(stepY);
}

} // namespace sim
