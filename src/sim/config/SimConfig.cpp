#include "sim/config/SimConfig.hpp"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <iostream>
#include <vector>

namespace sim {

int TerrainConfig::getMultiplier(TerrainType type) const {
    switch (type) {
        case TerrainType::Plain:    return plainMultiplier;
        case TerrainType::Forest:   return forestMultiplier;
        case TerrainType::Hill:     return hillMultiplier;
        case TerrainType::Mountain: return mountainMultiplier;
        case TerrainType::River:    return riverMultiplier;
        case TerrainType::Sea:      return seaMultiplier;
        case TerrainType::Lake:     return lakeMultiplier;
        default:                    return 0;
    }
}

bool TerrainConfig::isPassable(TerrainType type) const {
    return getMultiplier(type) > 0;
}

SimConfig::SimConfig() = default;

static bool fileExists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

static bool readFileToString(const std::string& path, std::string& outContent) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(f);
        return false;
    }
    outContent.resize(static_cast<size_t>(size));
    size_t readBytes = 0;
    if (size > 0) {
        readBytes = std::fread(&outContent[0], 1, static_cast<size_t>(size), f);
    }
    std::fclose(f);
    outContent.resize(readBytes);
    return true;
}

bool SimConfig::autoLoad() {
    std::vector<std::string> searchDirs = {
        "data",
        "../data",
        "../../data"
    };

    for (const auto& dir : searchDirs) {
        std::string testPath = dir + "/rules.json";
        if (fileExists(testPath)) {
            return loadFromDirectory(dir);
        }
    }

    std::cerr << "[SimConfig] LOI: Khong tim thay thu muc 'data/' chua cac file cau hinh!" << std::endl;
    return false;
}

bool SimConfig::loadFromDirectory(const std::string& dirPath) {
    // 1. Doc file data/rules.json
    std::string rulesPath = dirPath + "/rules.json";
    std::string rulesContent;
    if (!readFileToString(rulesPath, rulesContent)) {
        std::cerr << "[SimConfig] LOI: Khong the mo file '" << rulesPath << "'" << std::endl;
        return false;
    }

    try {
        nlohmann::json jRules = nlohmann::json::parse(rulesContent);
        if (!jRules.contains("meters_per_tile") || !jRules.contains("tick_per_second") || !jRules.contains("time_scale")) {
            std::cerr << "[SimConfig] LOI: File '" << rulesPath << "' thieu truong bat buoc (meters_per_tile, tick_per_second, time_scale)!" << std::endl;
            return false;
        }
        m_rules.metersPerTile = jRules["meters_per_tile"].get<int>();
        m_rules.tickPerSecond = jRules["tick_per_second"].get<int>();
        m_rules.timeScale = jRules["time_scale"].get<int>();
    } catch (const std::exception& e) {
        std::cerr << "[SimConfig] LOI phan tich JSON trong '" << rulesPath << "': " << e.what() << std::endl;
        return false;
    }

    // 2. Doc file data/terrain.json
    std::string terrainPath = dirPath + "/terrain.json";
    std::string terrainContent;
    if (!readFileToString(terrainPath, terrainContent)) {
        std::cerr << "[SimConfig] LOI: Khong the mo file '" << terrainPath << "'" << std::endl;
        return false;
    }

    try {
        nlohmann::json jTerrain = nlohmann::json::parse(terrainContent);
        if (!jTerrain.contains("infantry_speed_multiplier")) {
            std::cerr << "[SimConfig] LOI: File '" << terrainPath << "' thieu khoa 'infantry_speed_multiplier'!" << std::endl;
            return false;
        }
        const auto& sm = jTerrain["infantry_speed_multiplier"];
        m_terrain.plainMultiplier = sm.value("Plain", 100);
        m_terrain.forestMultiplier = sm.value("Forest", 60);
        m_terrain.hillMultiplier = sm.value("Hill", 50);
        m_terrain.mountainMultiplier = sm.value("Mountain", 25);
        m_terrain.riverMultiplier = sm.value("River", 20);
        m_terrain.seaMultiplier = sm.value("Sea", 0);
        m_terrain.lakeMultiplier = sm.value("Lake", 0);
    } catch (const std::exception& e) {
        std::cerr << "[SimConfig] LOI phan tich JSON trong '" << terrainPath << "': " << e.what() << std::endl;
        return false;
    }

    // 3. Doc file data/units.json
    std::string unitsPath = dirPath + "/units.json";
    std::string unitsContent;
    if (!readFileToString(unitsPath, unitsContent)) {
        std::cerr << "[SimConfig] LOI: Khong the mo file '" << unitsPath << "'" << std::endl;
        return false;
    }

    try {
        nlohmann::json jUnits = nlohmann::json::parse(unitsContent);
        if (!jUnits.contains("infantry_division")) {
            std::cerr << "[SimConfig] LOI: File '" << unitsPath << "' thieu dinh nghia 'infantry_division'!" << std::endl;
            return false;
        }
        const auto& inf = jUnits["infantry_division"];
        m_infantry.speedKmh = inf.value("speed_kmh", 5);
        m_infantry.manpower = inf.value("manpower", 3000);
        m_infantry.hp = inf.value("hp", 1000);
        m_infantry.attack = inf.value("attack", 0);
        m_infantry.armor = inf.value("armor", 0);
        m_infantry.range = inf.value("range", 0);
    } catch (const std::exception& e) {
        std::cerr << "[SimConfig] LOI phan tich JSON trong '" << unitsPath << "': " << e.what() << std::endl;
        return false;
    }

    std::cout << "[SimConfig] Nap thanh cong cau hinh tu '" << dirPath << "'\n"
              << "  Rules: " << m_rules.metersPerTile << " m/o, "
              << m_rules.tickPerSecond << " tick/s, time_scale " << m_rules.timeScale << "x\n"
              << "  Terrain multipliers: Plain=" << m_terrain.plainMultiplier
              << "%, Forest=" << m_terrain.forestMultiplier
              << "%, Hill=" << m_terrain.hillMultiplier
              << "%, Mountain=" << m_terrain.mountainMultiplier
              << "%, River=" << m_terrain.riverMultiplier << "%\n"
              << "  Infantry: " << m_infantry.speedKmh << " km/h, "
              << m_infantry.manpower << " quan" << std::endl;
    return true;
}

int SimConfig::calculateStepMilli(TerrainType terrain, int& accumulator) const {
    int mult = m_terrain.getMultiplier(terrain);
    if (mult <= 0) {
        return 0; // Dia hinh khong di duoc (bien, ho)
    }

    int64_t num = static_cast<int64_t>(m_infantry.speedKmh)
                * m_rules.timeScale
                * 25LL
                * mult;

    int64_t den = 9LL
                * m_rules.tickPerSecond
                * m_rules.metersPerTile;

    if (den <= 0) {
        return 0;
    }

    accumulator += static_cast<int>(num);
    int step = accumulator / static_cast<int>(den);
    accumulator %= static_cast<int>(den);
    return step;
}

int64_t SimConfig::calculateDistanceMilli(int ticks, TerrainType terrain) const {
    int mult = m_terrain.getMultiplier(terrain);
    if (mult <= 0 || ticks <= 0) {
        return 0;
    }

    int64_t num = static_cast<int64_t>(ticks)
                * m_infantry.speedKmh
                * m_rules.timeScale
                * 25LL
                * mult;

    int64_t den = 9LL
                * m_rules.tickPerSecond
                * m_rules.metersPerTile;

    return (den > 0) ? (num / den) : 0;
}

} // namespace sim
