#include "sim/config/SimConfig.hpp"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace sim {

static std::mutex g_configCacheMutex;
static std::unordered_map<std::string, SimConfig> g_configCache;

void SimConfig::clearCache() {
    std::lock_guard<std::mutex> lock(g_configCacheMutex);
    g_configCache.clear();
}

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

SimConfig::SimConfig() {
    // Khởi tạo trước mảng 8 loại địa hình
    m_terrains.resize(8);
}

bool SimConfig::autoLoad() {
    const std::vector<std::string> candidatePaths = {
        "data/game_config.json",
        "../data/game_config.json",
        "../../data/game_config.json"
    };

    for (const auto& path : candidatePaths) {
        if (fileExists(path)) {
            return loadFromFile(path);
        }
    }

    std::cerr << "[SimConfig] LOI: Khong tim thay file 'data/game_config.json'!" << std::endl;
    return false;
}

static UnitDomain parseDomain(const std::string& domainStr) {
    if (domainStr == "AIR") return UnitDomain::Air;
    if (domainStr == "SEA") return UnitDomain::Sea;
    return UnitDomain::Land;
}

bool SimConfig::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(g_configCacheMutex);
    auto it = g_configCache.find(filePath);
    if (it != g_configCache.end()) {
        *this = it->second;
        return true;
    }

    std::string content;
    if (!readFileToString(filePath, content)) {
        std::cerr << "[SimConfig] LOI: Khong the mo file cau hinh '" << filePath << "'!" << std::endl;
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(content);
    } catch (const std::exception& e) {
        std::cerr << "[SimConfig] LOI phan tich JSON trong '" << filePath << "': " << e.what() << std::endl;
        return false;
    }

    // 1. Kiem tra va nap Scale
    if (!root.contains("scale") || !root["scale"].is_object()) {
        std::cerr << "[SimConfig] LOI: File '" << filePath << "' thieu truong 'scale'!" << std::endl;
        return false;
    }
    const auto& jScale = root["scale"];
    m_scale.tileMeters = jScale.value("tileMeters", 50);
    m_scale.tilePixels = jScale.value("tilePixels", 32);
    m_scale.tickRate = jScale.value("tickRate", 20);
    m_matchDurationSec = root.value("matchDurationSec", 3600);
    if (root.contains("vision") && root["vision"].is_object()) {
        const auto& jVision = root["vision"];
        m_visionConfig.airElevation = jVision.value("airElevation", 4);
        m_visionConfig.commanderRevealTicks = jVision.value("commanderRevealTicks", 40);
    }
    if (jScale.contains("mapTiles") && jScale["mapTiles"].is_array() && jScale["mapTiles"].size() >= 2) {
        m_scale.mapWidth = jScale["mapTiles"][0].get<int>();
        m_scale.mapHeight = jScale["mapTiles"][1].get<int>();
    }

    // 2. Kiem tra va nap Terrain
    if (!root.contains("terrain") || !root["terrain"].is_object()) {
        std::cerr << "[SimConfig] LOI: File '" << filePath << "' thieu truong 'terrain'!" << std::endl;
        return false;
    }
    const auto& jTerrain = root["terrain"];
    const std::vector<std::pair<std::string, DetailedTerrainType>> terrainKeys = {
        {"DEEP_SEA", DetailedTerrainType::DeepSea},
        {"COASTAL_SEA", DetailedTerrainType::CoastalSea},
        {"PLAIN", DetailedTerrainType::Plain},
        {"FOREST", DetailedTerrainType::Forest},
        {"HILL", DetailedTerrainType::Hill},
        {"MOUNTAIN", DetailedTerrainType::Mountain},
        {"RIVER", DetailedTerrainType::River},
        {"FORD", DetailedTerrainType::Ford}
    };

    m_terrains.assign(8, TerrainProperty{});
    for (const auto& [k, type] : terrainKeys) {
        if (!jTerrain.contains(k)) {
            std::cerr << "[SimConfig] LOI: File '" << filePath << "' thieu thong tin dia hinh '" << k << "'!" << std::endl;
            return false;
        }
        const auto& tObj = jTerrain[k];
        int id = tObj.value("id", static_cast<int>(type));
        if (id < 0 || id >= 8) id = static_cast<int>(type);

        TerrainProperty prop;
        prop.name = tObj.value("name", k);
        prop.defBonus = tObj.value("defBonus", 0.0f);
        prop.visionMod = tObj.value("visionMod", 0);
        prop.rangeMod = tObj.value("rangeMod", 0);
        prop.elevation = tObj.value("elevation", 0);
        prop.ambush = tObj.value("ambush", false);

        if (tObj.contains("speed") && tObj["speed"].is_object()) {
            const auto& s = tObj["speed"];
            prop.speedInfantry = s.value("infantry", 0.0f);
            prop.speedVehicle = s.value("vehicle", 0.0f);
            prop.speedShip = s.value("ship", 0.0f);
            prop.speedAir = s.value("air", 1.0f);
        }
        m_terrains[static_cast<size_t>(id)] = prop;
    }

    // 3. Kiem tra va nap Units
    if (!root.contains("units") || !root["units"].is_object()) {
        std::cerr << "[SimConfig] LOI: File '" << filePath << "' thieu truong 'units'!" << std::endl;
        return false;
    }
    const auto& jUnits = root["units"];
    m_unitConfigs.clear();

    const std::vector<std::string> unitKeyList = {
        "COMMANDER", "INFANTRY", "LEOPARD_2A", "T14_ARMATA", "PUMA",
        "CAESAR", "GEPARD", "SU57", "B2_SPIRIT", "FFM3_MOGAMI", "DD151_ASAGIRI"
    };

    for (const auto& uKey : unitKeyList) {
        if (!jUnits.contains(uKey)) {
            std::cerr << "[SimConfig] LOI: File '" << filePath << "' thieu thong so don vi '" << uKey << "'!" << std::endl;
            return false;
        }
        const auto& uObj = jUnits[uKey];
        UnitTypeConfig cfg;
        cfg.key = uKey;
        cfg.name = uObj.value("name", uKey);
        cfg.domain = parseDomain(uObj.value("domain", "LAND"));
        cfg.unitsPerDot = uObj.value("unitsPerDot", 1);
        cfg.dotsPerSide = uObj.value("dotsPerSide", 1);
        cfg.totalPerSide = uObj.value("totalPerSide", 1);
        cfg.hpPerUnit = uObj.value("hpPerUnit", 100);
        cfg.armor = uObj.value("armor", 0);
        cfg.speedTilesPerSec = uObj.value("speedTilesPerSec", 1.0f);
        cfg.speedMilliPerSec = static_cast<int>(std::round(cfg.speedTilesPerSec * 1000.0f));
        cfg.rangeTiles = uObj.value("rangeTiles", 1.0f);
        cfg.visionTiles = uObj.value("visionTiles", 7);
        cfg.cooldownTicks = uObj.value("cooldownTicks", 20);
        cfg.canTargetAir = uObj.value("canTargetAir", false);
        cfg.canTargetGround = uObj.value("canTargetGround", true);
        cfg.canTargetSea = uObj.value("canTargetSea", false);
        cfg.stealthRevealRangeTiles = uObj.value("stealthRevealRangeTiles", 0.0f);
        cfg.iconSizeTiles = uObj.value("iconSizeTiles", 1);
        cfg.elevation = uObj.value("elevation", (cfg.domain == UnitDomain::Air ? 4 : 0));

        if (uObj.contains("weapons") && uObj["weapons"].is_array()) {
            for (const auto& w : uObj["weapons"]) {
                WeaponConfig wcfg;
                wcfg.ammo = w.value("ammo", "AP");
                wcfg.dmg = w.value("dmg", 0);
                wcfg.splash = w.value("splash", 0.0f);
                wcfg.rounds = w.value("rounds", 1);
                wcfg.accuracy = w.value("accuracy", 1.0f);
                wcfg.smallArms = w.value("smallArms", false);
                cfg.weapons.push_back(wcfg);
            }
        }
        m_unitConfigs.push_back(cfg);
    }

    std::cout << "[SimConfig] Nap thanh cong cau hinh tu '" << filePath << "' (11 loai don vi, 8 loai dia hinh)\n"
              << "  Scale: 1 o = " << m_scale.tileMeters << " m, "
              << m_scale.tilePixels << " px, ban do " << m_scale.mapWidth << "x" << m_scale.mapHeight
              << ", tickRate " << m_scale.tickRate << " tps" << std::endl;

    g_configCache[filePath] = *this;
    return true;
}

DetailedTerrainType SimConfig::classifyTerrain(const WorldMap& map, int tileX, int tileY) const {
    if (!map.isInBounds(tileX, tileY)) {
        return DetailedTerrainType::DeepSea;
    }

    TerrainType t = map.getTile(tileX, tileY);
    switch (t) {
        case TerrainType::Plain:
            return DetailedTerrainType::Plain;
        case TerrainType::Forest:
            return DetailedTerrainType::Forest;
        case TerrainType::Hill:
            return DetailedTerrainType::Hill;
        case TerrainType::Mountain:
            return DetailedTerrainType::Mountain;
        case TerrainType::River:
            return DetailedTerrainType::River;
        case TerrainType::Lake:
            // Quy tac 2: Lake cua M2 dung chi so COASTAL_SEA
            return DetailedTerrainType::CoastalSea;
        case TerrainType::Sea: {
            // Quy tac 2: Sea cua M2: o cach dat lien tu 3 o tro len la DEEP_SEA, con lai la COASTAL_SEA
            // Kiem tra trong ban kinh 2 o (khoang cach Chebyshev <= 2) co o dat lien nao khong
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = tileX + dx;
                    int ny = tileY + dy;
                    if (map.isInBounds(nx, ny)) {
                        TerrainType neighbor = map.getTile(nx, ny);
                        // Dat lien la bat ky loai nao khong phai Sea va khong phai Lake
                        if (neighbor != TerrainType::Sea && neighbor != TerrainType::Lake) {
                            return DetailedTerrainType::CoastalSea;
                        }
                    }
                }
            }
            return DetailedTerrainType::DeepSea;
        }
        default:
            return DetailedTerrainType::Plain;
    }
}

int SimConfig::getSpeedMultiplierPercent(DetailedTerrainType terrain, UnitDomain domain, UnitType type) const {
    size_t idx = static_cast<size_t>(terrain);
    if (idx >= m_terrains.size()) return 0;
    const auto& prop = m_terrains[idx];

    float sp = 0.0f;
    if (type == UnitType::Infantry) {
        sp = prop.speedInfantry;
    } else if (domain == UnitDomain::Air) {
        sp = prop.speedAir;
    } else if (domain == UnitDomain::Sea) {
        sp = prop.speedShip;
    } else {
        // Cac don vi mat dat khac: xe tang, thiet giap, phao, tu lenh
        sp = prop.speedVehicle;
    }

    return static_cast<int>(std::round(sp * 100.0f));
}

bool SimConfig::isPassable(DetailedTerrainType terrain, UnitDomain domain, UnitType type) const {
    return getSpeedMultiplierPercent(terrain, domain, type) > 0;
}

int SimConfig::calculateStepMilli(DetailedTerrainType terrain, UnitType type, int& accumulator) const {
    const UnitTypeConfig* cfg = getUnitConfig(type);
    if (!cfg) return 0;

    int multPercent = getSpeedMultiplierPercent(terrain, cfg->domain, type);
    if (multPercent <= 0) return 0;

    int num = cfg->speedMilliPerSec * multPercent;
    int den = 100 * m_scale.tickRate;
    if (den <= 0) return 0;

    accumulator += num;
    int step = accumulator / den;
    accumulator %= den;
    return step;
}

const UnitTypeConfig* SimConfig::getUnitConfig(UnitType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx < m_unitConfigs.size()) {
        return &m_unitConfigs[idx];
    }
    return nullptr;
}

const UnitTypeConfig* SimConfig::getUnitConfigByKey(const std::string& key) const {
    for (const auto& u : m_unitConfigs) {
        if (u.key == key) return &u;
    }
    return nullptr;
}

const TerrainProperty& SimConfig::getTerrainProperty(DetailedTerrainType type) const {
    size_t idx = static_cast<size_t>(type);
    if (idx < m_terrains.size()) {
        return m_terrains[idx];
    }
    static const TerrainProperty emptyProp{};
    return emptyProp;
}

} // namespace sim
