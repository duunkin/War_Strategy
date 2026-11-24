#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "sim/config/SimConfig.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/units/Unit.hpp"
#include "sim/WorldSim.hpp"
#include "mapgen/MapGenerator.hpp"
#include "mapgen/MapGenConfig.hpp"

#include <iostream>
#include <cmath>
#include <unordered_set>

int main() {
    std::cout << "==================================================\n";
    std::cout << " [TEST SUITE] Kiem tra Don vi & Dinh dang JSON M3 \n";
    std::cout << "==================================================\n";

    // -------------------------------------------------------------
    // Test 1: Nap file cau hinh JSON tu data/game_config.json
    // -------------------------------------------------------------
    std::cout << "\n[TEST 1] Nap cau hinh tu data/game_config.json..." << std::endl;
    sim::SimConfig config;
    if (!config.autoLoad()) {
        std::cerr << "-> THAT BAI: Khong the tu dong nap cau hinh tu data/game_config.json!" << std::endl;
        return 1;
    }

    // Kiem tra Scale
    assert(config.getScale().tileMeters == 50);
    assert(config.getScale().tilePixels == 32);
    assert(config.getScale().mapWidth == 128);
    assert(config.getScale().mapHeight == 96);
    assert(config.getScale().tickRate == 20);

    // Kiem tra du 11 loai don vi
    assert(config.getAllUnitConfigs().size() == 11);

    const auto* infCfg = config.getUnitConfig(sim::UnitType::Infantry);
    assert(infCfg != nullptr);
    assert(infCfg->unitsPerDot == 3000);
    assert(infCfg->dotsPerSide == 10);
    assert(infCfg->hpPerUnit == 100);
    assert(infCfg->armor == 8);
    assert(infCfg->speedTilesPerSec == 2.13f);
    assert(infCfg->speedMilliPerSec == 2130);

    const auto* cmdCfg = config.getUnitConfig(sim::UnitType::Commander);
    assert(cmdCfg != nullptr);
    assert(cmdCfg->unitsPerDot == 1);
    assert(cmdCfg->hpPerUnit == 100);
    assert(cmdCfg->armor == 0);
    assert(cmdCfg->speedTilesPerSec == 2.46f);
    assert(cmdCfg->speedMilliPerSec == 2460);

    const auto* tankCfg = config.getUnitConfig(sim::UnitType::Leopard2A);
    assert(tankCfg != nullptr);
    assert(tankCfg->unitsPerDot == 40);
    assert(tankCfg->hpPerUnit == 2000);
    assert(tankCfg->armor == 68);

    const auto* shipCfg = config.getUnitConfig(sim::UnitType::Mogami);
    assert(shipCfg != nullptr);
    assert(shipCfg->domain == sim::UnitDomain::Sea);
    assert(shipCfg->unitsPerDot == 10);
    assert(shipCfg->hpPerUnit == 4000);

    std::cout << "-> TEST 1 THANH CONG: Du lieu JSON 11 loai don vi va scale nap chinh xac!" << std::endl;

    // -------------------------------------------------------------
    // Test 2: Kiem tra anh xa dia hinh tu WorldMap sang DetailedTerrainType
    // -------------------------------------------------------------
    std::cout << "\n[TEST 2] Kiem tra anh xa dia hinh (Deep Sea, Coastal Sea, Lake, Plain)..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Sea);

        // Dat 1 o dat lien Plain o (10, 10)
        map.setTile(10, 10, sim::TerrainType::Plain);

        // O (10, 11): cach Plain 1 o -> phai la CoastalSea
        assert(config.classifyTerrain(map, 10, 11) == sim::DetailedTerrainType::CoastalSea);
        // O (10, 12): cach Plain 2 o -> phai la CoastalSea
        assert(config.classifyTerrain(map, 10, 12) == sim::DetailedTerrainType::CoastalSea);
        // O (10, 13): cach Plain 3 o -> phai la DeepSea
        assert(config.classifyTerrain(map, 10, 13) == sim::DetailedTerrainType::DeepSea);
        // O (10, 20): cach xa Plain -> phai la DeepSea
        assert(config.classifyTerrain(map, 10, 20) == sim::DetailedTerrainType::DeepSea);

        // O Lake -> phai la CoastalSea theo quy tac 2
        map.setTile(30, 30, sim::TerrainType::Lake);
        assert(config.classifyTerrain(map, 30, 30) == sim::DetailedTerrainType::CoastalSea);

        // He so toc do:
        // Bo binh tren Plain = 100%, tren Forest = 90%, tren DeepSea = 0%
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::Plain, sim::UnitDomain::Land, sim::UnitType::Infantry) == 100);
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::Forest, sim::UnitDomain::Land, sim::UnitType::Infantry) == 90);
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::DeepSea, sim::UnitDomain::Land, sim::UnitType::Infantry) == 0);

        // Tau tren DeepSea = 100%, CoastalSea = 90%, Plain = 0%
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::DeepSea, sim::UnitDomain::Sea, sim::UnitType::Mogami) == 100);
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::CoastalSea, sim::UnitDomain::Sea, sim::UnitType::Mogami) == 90);
        assert(config.getSpeedMultiplierPercent(sim::DetailedTerrainType::Plain, sim::UnitDomain::Sea, sim::UnitType::Mogami) == 0);
    }
    std::cout << "-> TEST 2 THANH CONG: Anh xa dia hinh Deep Sea / Coastal Sea / Lake hoat dong chuan!" << std::endl;

    // -------------------------------------------------------------
    // Test 3: Kiem tra sinh 30 cham moi ben (Player phia Tay, Enemy phia Dong doi xung)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 3] Kiem tra sinh 30 cham Player va 30 cham Enemy doi xung..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        // Tao ban do hop le co bien bao quanh va dat bang o giua
        map.fill(sim::TerrainType::Sea);
        for (int y = 10; y < 86; ++y) {
            for (int x = 10; x < 118; ++x) {
                map.setTile(x, y, sim::TerrainType::Plain);
            }
        }

        sim::WorldSim sim(map);
        sim.loadConfig();

        uint64_t seed = 123456789ULL;
        sim.spawnDivisions(seed);

        const auto& units = sim.getUnits();
        // Tong cong 30 + 30 = 60 cham
        assert(units.size() == 60);

        int playerCount = 0;
        int enemyCount = 0;
        for (const auto& u : units) {
            if (u.getFaction() == sim::Faction::Player) {
                playerCount++;
                // Toan bo Player phai nam o nua Tay (x < 64)
                assert(u.getTileX() < 64);
            } else if (u.getFaction() == sim::Faction::Enemy) {
                enemyCount++;
                // Toan bo Enemy phai nam o nua Dong (x >= 64)
                assert(u.getTileX() >= 64);
            }

            // Kiem tra don vi luon nam tren o di duoc cua dung mien
            sim::DetailedTerrainType dt = config.classifyTerrain(map, u.getTileX(), u.getTileY());
            bool passable = config.isPassable(dt, u.getDomain(), u.getType());
            assert(passable == true);
        }

        assert(playerCount == 30);
        assert(enemyCount == 30);

        // Kiem tra tinh tat dinh
        sim::WorldSim sim2(map);
        sim2.loadConfig();
        sim2.spawnDivisions(seed);
        assert(sim2.getUnits().size() == 60);
        for (size_t i = 0; i < 60; ++i) {
            assert(units[i].getId() == sim2.getUnits()[i].getId());
            assert(units[i].getPosX() == sim2.getUnits()[i].getPosX());
            assert(units[i].getPosY() == sim2.getUnits()[i].getPosY());
            assert(units[i].getType() == sim2.getUnits()[i].getType());
            assert(units[i].getFaction() == sim2.getUnits()[i].getFaction());
        }
    }
    std::cout << "-> TEST 3 THANH CONG: Sinh du 60 cham (30 Player Tay, 30 Enemy Dong) tat dinh va dung dia hinh!" << std::endl;

    // -------------------------------------------------------------
    // Test 4: Kiem tra takeDamage, giam so quan va xoa cham khi het mau
    // -------------------------------------------------------------
    std::cout << "\n[TEST 4] Kiem tra takeDamage, cap nhat quan so va xoa cham khi chet..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain);
        sim::WorldSim sim(map);
        sim.loadConfig();

        const auto* infCfg = sim.getConfig().getUnitConfig(sim::UnitType::Infantry);
        assert(infCfg != nullptr);

        // Tao 1 don vi bo binh: 3000 quan, 100 hp/quan -> tong hp = 300,000
        sim.getUnits().emplace_back(1, sim::Faction::Player, sim::UnitType::Infantry, 10000, 10000, *infCfg);

        auto* u = sim.getUnitById(1);
        assert(u != nullptr);
        assert(u->getHp() == 300000);
        assert(u->getCurrentUnits() == 3000);
        assert(u->isDead() == false);

        // Ban mat 50 mau -> hp = 299950 -> ceil(299950 / 100) = 3000 quan
        u->takeDamage(50);
        assert(u->getHp() == 299950);
        assert(u->getCurrentUnits() == 3000);

        // Ban tiep 50 mau -> hp = 299900 -> ceil(299900 / 100) = 2999 quan (thuong vong 1 quan)
        u->takeDamage(50);
        assert(u->getHp() == 299900);
        assert(u->getCurrentUnits() == 2999);

        // Ban mat 199,900 mau -> hp = 100,000 -> 1000 quan
        u->takeDamage(199900);
        assert(u->getHp() == 100000);
        assert(u->getCurrentUnits() == 1000);

        // Ban ket lieu 100,000 mau -> hp = 0 -> 0 quan -> isDead = true
        u->takeDamage(100000);
        assert(u->getHp() == 0);
        assert(u->getCurrentUnits() == 0);
        assert(u->isDead() == true);

        // Truoc khi tick, don vi van con trong vector
        assert(sim.getUnits().size() == 1);

        // Chay tick: don vi da chet phai bi xoa khoi danh sach cham
        sim.tick();
        assert(sim.getUnits().empty());
        assert(sim.getUnitById(1) == nullptr);
    }
    std::cout << "-> TEST 4 THANH CONG: Cong thuc mau, thuong vong va xoa cham khi ve 0 hoan toan chinh xac!" << std::endl;

    // -------------------------------------------------------------
    // Test 5: Kiem tra buoc di va quang duong di chuyen theo speedTilesPerSec
    // -------------------------------------------------------------
    std::cout << "\n[TEST 5] Kiem tra toc do di chuyen theo speedTilesPerSec..." << std::endl;
    {
        // Bo binh speedTilesPerSec = 2.13 -> 2130 mili-o/s
        // Tren Plain (100%): 20 tick di duoc dung 2130 mili-o
        int accPlain = 0;
        int totalPlain = 0;
        for (int t = 0; t < 20; ++t) {
            totalPlain += config.calculateStepMilli(sim::DetailedTerrainType::Plain, sim::UnitType::Infantry, accPlain);
        }
        assert(totalPlain == 2130);

        // Tren Forest (90%): 2130 * 90 / 100 = 1917 mili-o/s
        int accForest = 0;
        int totalForest = 0;
        for (int t = 0; t < 20; ++t) {
            totalForest += config.calculateStepMilli(sim::DetailedTerrainType::Forest, sim::UnitType::Infantry, accForest);
        }
        assert(totalForest == 1917);
    }
    std::cout << "-> TEST 5 THANH CONG: Tich luy toc do di chuyen 20 tick/s bao toan 100% so nguyen!" << std::endl;

    // -------------------------------------------------------------
    // Test 6: Kiem tra vi tri spawn co dinh o 2 trung diem canh ben tren 20 seeds
    // -------------------------------------------------------------
    std::cout << "\n[TEST 6] Kiem tra spawn co dinh tai 2 trung diem canh ben tren 20 seed thuc te..." << std::endl;
    {
        mapgen::MapGenerator generator;
        mapgen::MapGenConfig mapCfg;
        sim::WorldMap realMap(128, 96, 32);

        const uint64_t testSeeds[20] = {
            1001ULL, 2022ULL, 3033ULL, 4044ULL, 5055ULL,
            6066ULL, 7077ULL, 8088ULL, 9099ULL, 12345ULL,
            54321ULL, 987654ULL, 1337ULL, 7777ULL, 88888ULL,
            999999ULL, 11223344ULL, 55667788ULL, 99001122ULL, 31415926ULL
        };

        constexpr int expectedMidY = 48; // 96 / 2

        for (int sIdx = 0; sIdx < 20; ++sIdx) {
            uint64_t seed = testSeeds[sIdx];
            generator.generate(realMap, seed, mapCfg);

            sim::WorldSim sim(realMap);
            assert(sim.loadConfig());
            sim.spawnDivisions(seed);

            const auto& units = sim.getUnits();
            assert(units.size() == 60);

            // Kiểm tra Anchor có nằm gần midY không
            int pAnchorX = sim.getPlayerSpawnAnchorX();
            int pAnchorY = sim.getPlayerSpawnAnchorY();
            int eAnchorX = sim.getEnemySpawnAnchorX();
            int eAnchorY = sim.getEnemySpawnAnchorY();

            assert(pAnchorX >= 2 && pAnchorX < 64);
            assert(eAnchorX >= 64 && eAnchorX < 126);
            assert(std::abs(pAnchorY - expectedMidY) <= 8 && "Player anchor phai rat gan midY = 48!");
            assert(std::abs(eAnchorY - expectedMidY) <= 8 && "Enemy anchor phai rat gan midY = 48!");

            // Kiểm tra từng đơn vị của Player và Enemy
            int playerSumX = 0, playerSumY = 0;
            int enemySumX = 0, enemySumY = 0;
            std::unordered_set<int> playerTileSet;
            std::unordered_set<int> enemyTileSet;

            for (const auto& u : units) {
                int tx = u.getTileX();
                int ty = u.getTileY();
                int tileKey = ty * 1000 + tx;

                sim::DetailedTerrainType dt = config.classifyTerrain(realMap, tx, ty);
                assert(config.isPassable(dt, u.getDomain(), u.getType()) &&
                       "Don vi phai dung tren o dia hinh hoan toan hop le!");

                if (u.getDomain() == sim::UnitDomain::Sea) {
                    assert((dt == sim::DetailedTerrainType::DeepSea || dt == sim::DetailedTerrainType::CoastalSea) &&
                           "Tau chien phai o tren bien!");
                } else if (u.getDomain() == sim::UnitDomain::Land) {
                    assert(dt != sim::DetailedTerrainType::DeepSea && dt != sim::DetailedTerrainType::CoastalSea &&
                           dt != sim::DetailedTerrainType::Mountain &&
                           "Luc quan khong duoc o tren bien hoac tren nui!");
                }

                if (u.getFaction() == sim::Faction::Player) {
                    assert(tx < 64 && "Player bat buoc o nua trai (Tay)!");
                    playerSumX += tx;
                    playerSumY += ty;
                    assert(playerTileSet.find(tileKey) == playerTileSet.end() && "Khong duoc de quan Player trung o!");
                    playerTileSet.insert(tileKey);
                } else if (u.getFaction() == sim::Faction::Enemy) {
                    assert(tx >= 64 && "Enemy bat buoc o nua phai (Dong)!");
                    enemySumX += tx;
                    enemySumY += ty;
                    assert(enemyTileSet.find(tileKey) == enemyTileSet.end() && "Khong duoc de quan Enemy trung o!");
                    enemyTileSet.insert(tileKey);
                }
            }

            assert(playerTileSet.size() == 30);
            assert(enemyTileSet.size() == 30);

            float playerAvgY = static_cast<float>(playerSumY) / 30.0f;
            float enemyAvgY = static_cast<float>(enemySumY) / 30.0f;
            assert(std::abs(playerAvgY - static_cast<float>(expectedMidY)) <= 8.0f &&
                   "Toa do Y trung binh cua quan Player phai tap trung quanh midY = 48!");
            assert(std::abs(enemyAvgY - static_cast<float>(expectedMidY)) <= 8.0f &&
                   "Toa do Y trung binh cua quan Enemy phai tap trung quanh midY = 48!");
        }
    }
    std::cout << "-> TEST 6 THANH CONG: 20/20 seed thuc te deu spawn chuan xac tai 2 trung diem canh ben!" << std::endl;

    std::cout << "\n==================================================\n";
    std::cout << " ALL TESTS PASSED! MOC M3 JSON HOAN TOAN CHUAN XAC!\n";
    std::cout << "==================================================\n";
    return 0;
}
