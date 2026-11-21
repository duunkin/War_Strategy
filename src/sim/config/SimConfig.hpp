#pragma once

#include "../map/WorldMap.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace sim {

/**
 * @brief Miền hoạt động của đơn vị
 */
enum class UnitDomain : uint8_t {
    Land = 0,
    Air = 1,
    Sea = 2
};

/**
 * @brief Phân loại địa hình chi tiết theo JSON (8 loại)
 */
enum class DetailedTerrainType : uint8_t {
    DeepSea = 0,    // DEEP_SEA
    CoastalSea = 1, // COASTAL_SEA
    Plain = 2,      // PLAIN
    Forest = 3,     // FOREST
    Hill = 4,       // HILL
    Mountain = 5,   // MOUNTAIN
    River = 6,      // RIVER
    Ford = 7        // FORD (Bến cạn)
};

/**
 * @brief Chủng loại đơn vị (11 loại trong game_config.json)
 */
enum class UnitType : uint8_t {
    Commander = 0,
    Infantry = 1,
    Leopard2A = 2,
    T14Armata = 3,
    Puma = 4,
    Caesar = 5,
    Gepard = 6,
    Su57 = 7,
    B2Spirit = 8,
    Mogami = 9,
    Asagiri = 10,
    Count = 11
};

/**
 * @brief Cấu hình vũ khí của một loại đơn vị
 */
struct WeaponConfig {
    std::string ammo{"AP"};
    int dmg{0};
    float splash{0.0f};
    int rounds{1};
    float accuracy{1.0f};
    bool smallArms{false};
};

/**
 * @brief Cấu hình chỉ số của từng loại đơn vị nạp từ JSON
 */
struct UnitTypeConfig {
    std::string key;
    std::string name;
    UnitDomain domain{UnitDomain::Land};
    int unitsPerDot{1};
    int dotsPerSide{1};
    int totalPerSide{1};
    int hpPerUnit{100};
    int armor{0};
    float speedTilesPerSec{1.0f};
    int speedMilliPerSec{1000};
    float rangeTiles{1.0f};
    int visionTiles{7};
    int cooldownTicks{20};
    std::vector<WeaponConfig> weapons;
    bool canTargetAir{false};
    bool canTargetGround{true};
    bool canTargetSea{false};
    float stealthRevealRangeTiles{0.0f};
    int iconSizeTiles{1};
    int elevation{0};
};

/**
 * @brief Hệ số tốc độ và đặc tính địa hình từ JSON
 */
struct TerrainProperty {
    std::string name;
    float speedInfantry{0.0f};
    float speedVehicle{0.0f};
    float speedShip{0.0f};
    float speedAir{1.0f};
    float defBonus{0.0f};
    int visionMod{0};
    int rangeMod{0};
    int elevation{0};
    bool ambush{false};
};

/**
 * @brief Cấu hình hệ thống tầm nhìn
 */
struct VisionConfig {
    int airElevation{4};
    int commanderRevealTicks{40};
};

/**
 * @brief Quy chuẩn tỉ lệ thế giới từ JSON
 */
struct ScaleConfig {
    int tileMeters{50};      // 1 ô = 50 m
    int tilePixels{32};      // 1 ô = 32 pixel
    int mapWidth{128};       // Bản đồ 128 ô ngang
    int mapHeight{96};       // Bản đồ 96 ô dọc
    int tickRate{20};        // 20 tick / giây
};

/**
 * @brief Lớp quản lý cấu hình game đọc duy nhất từ data/game_config.json
 */
class SimConfig {
public:
    SimConfig();
    ~SimConfig() = default;

    // Tự động tìm và đọc cấu hình từ data/game_config.json
    bool autoLoad();

    // Đọc cấu hình từ đường dẫn thư mục hoặc file cụ thể
    bool loadFromFile(const std::string& filePath);

    // Ánh xạ địa hình từ WorldMap (M2) sang DetailedTerrainType (JSON)
    // Sea: cách đất liền >= 3 ô là DEEP_SEA, còn lại COASTAL_SEA
    // Lake: COASTAL_SEA
    // Plain, Forest, Hill, Mountain, River: ánh xạ trực tiếp
    [[nodiscard]] DetailedTerrainType classifyTerrain(const WorldMap& map, int tileX, int tileY) const;

    // Lấy hệ số tốc độ (phần trăm 0..100) theo loại địa hình chi tiết, domain và UnitType
    [[nodiscard]] int getSpeedMultiplierPercent(DetailedTerrainType terrain, UnitDomain domain, UnitType type) const;

    // Kiểm tra đơn vị có thể di chuyển trên địa hình hay không
    [[nodiscard]] bool isPassable(DetailedTerrainType terrain, UnitDomain domain, UnitType type) const;

    // Tính bước di chuyển (mili-ô) trong 1 tick dùng accumulator
    [[nodiscard]] int calculateStepMilli(DetailedTerrainType terrain, UnitType type, int& accumulator) const;

    [[nodiscard]] const ScaleConfig& getScale() const { return m_scale; }
    [[nodiscard]] const std::vector<UnitTypeConfig>& getAllUnitConfigs() const { return m_unitConfigs; }
    [[nodiscard]] const UnitTypeConfig* getUnitConfig(UnitType type) const;
    [[nodiscard]] const UnitTypeConfig* getUnitConfigByKey(const std::string& key) const;
    [[nodiscard]] const TerrainProperty& getTerrainProperty(DetailedTerrainType type) const;

    [[nodiscard]] int getTickRate() const { return m_scale.tickRate; }
    [[nodiscard]] int getTilePixels() const { return m_scale.tilePixels; }
    [[nodiscard]] int getTileMeters() const { return m_scale.tileMeters; }
    [[nodiscard]] int getMatchDurationSec() const { return m_matchDurationSec; }
    [[nodiscard]] const VisionConfig& getVisionConfig() const { return m_visionConfig; }

private:
    ScaleConfig m_scale;
    int m_matchDurationSec{3600};
    VisionConfig m_visionConfig;
    std::vector<TerrainProperty> m_terrains; // 8 loại theo id 0..7
    std::vector<UnitTypeConfig>  m_unitConfigs;
};

} // namespace sim
