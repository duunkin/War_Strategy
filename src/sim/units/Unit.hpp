#pragma once

#include "../config/SimConfig.hpp"
#include "../pathfinding/Pathfinder.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

/**
 * @brief Phe phái của đơn vị
 */
enum class Faction : uint8_t {
    Neutral = 0,
    Player = 1,  // Phe người chơi (Xanh)
    Enemy = 2    // Phe đối phương (Đỏ)
};

/**
 * @brief Lớp Unit đại diện cho một chấm / khối quân trên bản đồ.
 *
 * Mốc M4 bổ sung:
 * - Quản lý lộ trình tìm đường A* qua các waypoints.
 * - Đích đến thực tế và cờ đánh dấu đích đã được điều chỉnh.
 * - Di chuyển theo từng waypoint với tốc độ theo địa hình ô đang đứng.
 */
class Unit {
public:
    Unit() = default;
    Unit(uint32_t id, Faction faction, UnitType type, int32_t posX, int32_t posY, const UnitTypeConfig& config);
    ~Unit() = default;

    // Định danh & phân loại
    [[nodiscard]] uint32_t getId() const { return m_id; }
    [[nodiscard]] Faction getFaction() const { return m_faction; }
    [[nodiscard]] UnitType getType() const { return m_type; }
    [[nodiscard]] UnitDomain getDomain() const { return m_domain; }
    [[nodiscard]] const std::string& getName() const { return m_name; }

    // Quân số & Máu
    [[nodiscard]] int getHp() const { return m_hp; }
    [[nodiscard]] int getMaxHp() const { return m_maxHp; }
    [[nodiscard]] int getCurrentUnits() const { return m_currentUnits; }
    [[nodiscard]] int getMaxUnits() const { return m_maxUnits; }
    [[nodiscard]] int getHpPerUnit() const { return m_hpPerUnit; }
    [[nodiscard]] int getManpower() const { return m_currentUnits; } // Tương thích ngược

    // Trừ máu khi chịu sát thương và cập nhật số đơn vị
    void takeDamage(int damage);
    [[nodiscard]] bool isDead() const { return m_hp <= 0 || m_currentUnits <= 0; }

    // Chỉ số chiến đấu & di chuyển
    [[nodiscard]] int getArmor() const { return m_armor; }
    [[nodiscard]] float getSpeedTilesPerSec() const { return m_speedTilesPerSec; }
    [[nodiscard]] int getSpeedMilliPerSec() const { return m_speedMilliPerSec; }
    [[nodiscard]] float getRangeTiles() const { return m_rangeTiles; }
    [[nodiscard]] int getVisionTiles() const { return m_visionTiles; }
    [[nodiscard]] int getCooldownTicks() const { return m_cooldownTicks; }
    [[nodiscard]] const std::vector<WeaponConfig>& getWeapons() const { return m_weapons; }

    // Khả năng nhắm mục tiêu & hiển thị
    [[nodiscard]] bool canTargetAir() const { return m_canTargetAir; }
    [[nodiscard]] bool canTargetGround() const { return m_canTargetGround; }
    [[nodiscard]] bool canTargetSea() const { return m_canTargetSea; }
    [[nodiscard]] float getStealthRevealRangeTiles() const { return m_stealthRevealRangeTiles; }
    [[nodiscard]] int getIconSizeTiles() const { return m_iconSizeTiles; }

    // Vị trí tính bằng mili-ô
    [[nodiscard]] int32_t getPosX() const { return m_posX; }
    [[nodiscard]] int32_t getPosY() const { return m_posY; }
    void setPositionMilli(int32_t x, int32_t y);

    // Tọa độ ô nguyên trên bản đồ
    [[nodiscard]] int getTileX() const { return m_posX / 1000; }
    [[nodiscard]] int getTileY() const { return m_posY / 1000; }

    // Đích đến tính bằng mili-ô
    [[nodiscard]] int32_t getDestX() const { return m_destX; }
    [[nodiscard]] int32_t getDestY() const { return m_destY; }
    [[nodiscard]] bool hasDestination() const { return m_hasDestination; }

    void setDestinationTile(int tileX, int tileY);
    void setDestinationMilli(int32_t destX, int32_t destY);
    void clearDestination();

    // Quản lý đường đi và waypoint (M4)
    void setPath(const PathResult& path);
    void clearPath();
    [[nodiscard]] bool hasPath() const { return !m_waypoints.empty() && m_currentWaypointIdx < m_waypoints.size(); }
    [[nodiscard]] const std::vector<TileCoord>& getWaypoints() const { return m_waypoints; }
    [[nodiscard]] size_t getCurrentWaypointIndex() const { return m_currentWaypointIdx; }
    [[nodiscard]] bool isDestinationAdjusted() const { return m_destinationAdjusted; }
    [[nodiscard]] TileCoord getFinalTargetTile() const { return m_finalTargetTile; }

    // Di chuyển thẳng về phía đích một đoạn stepMilli (thuần số nguyên)
    void stepTowardsDestination(int stepMilli);

    // Bộ tích lũy quãng đường sub-milli
    int& getMoveAccumulator() { return m_moveAccumulator; }
    [[nodiscard]] int getMoveAccumulator() const { return m_moveAccumulator; }

    // Trạng thái chọn bằng chuột
    [[nodiscard]] bool isSelected() const { return m_selected; }
    void setSelected(bool sel) { m_selected = sel; }

private:
    uint32_t    m_id{0};
    Faction     m_faction{Faction::Player};
    UnitType    m_type{UnitType::Infantry};
    UnitDomain  m_domain{UnitDomain::Land};
    std::string m_name;

    int32_t     m_posX{0};
    int32_t     m_posY{0};
    int32_t     m_destX{0};
    int32_t     m_destY{0};
    bool        m_hasDestination{false};
    int         m_moveAccumulator{0};
    bool        m_selected{false};

    // Máu và quân số
    int         m_hpPerUnit{100};
    int         m_maxUnits{1};
    int         m_currentUnits{1};
    int         m_hp{100};
    int         m_maxHp{100};

    // Chỉ số chiến đấu
    int         m_armor{0};
    float       m_speedTilesPerSec{1.0f};
    int         m_speedMilliPerSec{1000};
    float       m_rangeTiles{1.0f};
    int         m_visionTiles{7};
    int         m_cooldownTicks{20};
    std::vector<WeaponConfig> m_weapons;

    bool        m_canTargetAir{false};
    bool        m_canTargetGround{true};
    bool        m_canTargetSea{false};
    float       m_stealthRevealRangeTiles{0.0f};
    int         m_iconSizeTiles{1};

    // Waypoints và điều chỉnh đích (M4)
    std::vector<TileCoord> m_waypoints;
    size_t                 m_currentWaypointIdx{0};
    bool                   m_destinationAdjusted{false};
    TileCoord              m_finalTargetTile{0, 0};
};

} // namespace sim
