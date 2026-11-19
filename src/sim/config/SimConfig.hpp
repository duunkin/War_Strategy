#pragma once

#include "../map/WorldMap.hpp"
#include <cstdint>
#include <string>

namespace sim {

/**
 * @brief Cấu hình quy tắc trò chơi (đọc từ data/rules.json)
 */
struct RulesConfig {
    int metersPerTile{100};   // Chiều dài mỗi ô tính bằng mét
    int tickPerSecond{20};    // Số tick mô phỏng mỗi giây thực
    int timeScale{10};        // Tỉ lệ thời gian (1s thực = timeScale s game)
};

/**
 * @brief Hệ số tốc độ bộ binh theo loại địa hình (đọc từ data/terrain.json)
 * Giá trị là số nguyên phần trăm (ví dụ: đất bằng 100, rừng 60, biển 0 = không đi được)
 */
struct TerrainConfig {
    int plainMultiplier{100};
    int forestMultiplier{60};
    int hillMultiplier{50};
    int mountainMultiplier{25};
    int riverMultiplier{20};
    int seaMultiplier{0};
    int lakeMultiplier{0};

    [[nodiscard]] int getMultiplier(TerrainType type) const;
    [[nodiscard]] bool isPassable(TerrainType type) const;
};

/**
 * @brief Chỉ số đơn vị bộ binh (đọc từ data/units.json)
 */
struct UnitTypeConfig {
    int speedKmh{5};      // Tốc độ di chuyển cơ sở (km/h)
    int manpower{3000};   // Số quân của một sư đoàn
    int hp{1000};         // Máu
    int attack{0};        // Sát thương
    int armor{0};         // Giáp
    int range{0};         // Tầm bắn
};

/**
 * @brief Quản lý cấu hình toàn bộ hệ thống mô phỏng sim
 */
class SimConfig {
public:
    SimConfig();
    ~SimConfig() = default;

    // Tự động tìm và đọc cấu hình từ thư mục data/ (thử lần lượt data/, ../data/, ../../data/)
    bool autoLoad();

    // Đọc cấu hình từ thư mục cụ thể
    bool loadFromDirectory(const std::string& dirPath);

    // Tính toán quãng đường di chuyển (mili-ô) cho một tick theo địa hình và bộ tích lũy
    // Trả về số mili-ô di chuyển trong tick này và cập nhật accumulator
    // Công thức chi tiết được ghi trong phần hiện thực SimConfig.cpp
    [[nodiscard]] int calculateStepMilli(TerrainType terrain, int& accumulator) const;

    // Tính tổng quãng đường di chuyển lý thuyết (mili-ô) sau số lượng ticks cụ thể
    [[nodiscard]] int64_t calculateDistanceMilli(int ticks, TerrainType terrain) const;

    [[nodiscard]] const RulesConfig& getRules() const { return m_rules; }
    [[nodiscard]] const TerrainConfig& getTerrain() const { return m_terrain; }
    [[nodiscard]] const UnitTypeConfig& getInfantry() const { return m_infantry; }

private:
    RulesConfig    m_rules;
    TerrainConfig  m_terrain;
    UnitTypeConfig m_infantry;
};

} // namespace sim
