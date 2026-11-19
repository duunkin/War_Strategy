#include "Unit.hpp"
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

Unit::Unit(uint32_t id, Faction faction, UnitType type, int32_t posX, int32_t posY, int manpower)
    : m_id(id), m_faction(faction), m_type(type), m_posX(posX), m_posY(posY), m_manpower(manpower) {}

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
