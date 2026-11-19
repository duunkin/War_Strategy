#pragma once

#include <cstdint>

namespace sim {

/**
 * @brief Phe phái của đơn vị
 */
enum class Faction : uint8_t {
    Neutral = 0,
    Player = 1,  // Phe người chơi
    Enemy = 2    // Phe đối phương
};

/**
 * @brief Chủng loại đơn vị
 */
enum class UnitType : uint8_t {
    InfantryDivision = 1 // Sư đoàn bộ binh
};

/**
 * @brief Struct/Class Unit đại diện cho một sư đoàn quân đội trên bản đồ.
 *
 * Lưu ý quan trọng:
 * - Module sim không dùng float, không include SDL.
 * - Tọa độ vị trí (posX, posY) và đích đến (destX, destY) tính bằng mili-ô (1 ô = 1000 đơn vị).
 */
class Unit {
public:
    Unit() = default;
    Unit(uint32_t id, Faction faction, UnitType type, int32_t posX, int32_t posY, int manpower = 3000);
    ~Unit() = default;

    // Lấy thông tin cơ bản
    [[nodiscard]] uint32_t getId() const { return m_id; }
    [[nodiscard]] Faction getFaction() const { return m_faction; }
    [[nodiscard]] UnitType getType() const { return m_type; }
    [[nodiscard]] int getManpower() const { return m_manpower; }

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

    // Di chuyển thẳng về phía đích một đoạn stepMilli (thuần số nguyên)
    void stepTowardsDestination(int stepMilli);

    // Bộ tích lũy quãng đường sub-milli để bảo toàn độ chính xác số nguyên
    int& getMoveAccumulator() { return m_moveAccumulator; }
    [[nodiscard]] int getMoveAccumulator() const { return m_moveAccumulator; }

    // Trạng thái chọn bằng chuột
    [[nodiscard]] bool isSelected() const { return m_selected; }
    void setSelected(bool sel) { m_selected = sel; }

private:
    uint32_t m_id{0};                        // Mã định danh đơn vị
    Faction  m_faction{Faction::Player};     // Phe phái
    UnitType m_type{UnitType::InfantryDivision}; // Chủng loại đơn vị
    int32_t  m_posX{0};                      // Tọa độ X (mili-ô)
    int32_t  m_posY{0};                      // Tọa độ Y (mili-ô)
    int32_t  m_destX{0};                     // Đích đến X (mili-ô)
    int32_t  m_destY{0};                     // Đích đến Y (mili-ô)
    bool     m_hasDestination{false};        // Có đang di chuyển tới đích hay không
    int      m_manpower{3000};               // Quân số (mặc định 3.000 quân/sư đoàn)

    int      m_moveAccumulator{0};           // Bộ tích lũy số dư tốc độ
    bool     m_selected{false};              // Cờ đánh dấu đang được chọn
};

} // namespace sim
