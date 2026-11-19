#include "sim/config/SimConfig.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/units/Unit.hpp"
#include "sim/WorldSim.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "==================================================\n";
    std::cout << " [TEST SUITE] Kiem tra Don vi & Di chuyen (Moc M3a)\n";
    std::cout << "==================================================\n";

    // -------------------------------------------------------------
    // Test 1: Nap file cau hinh JSON tu thu muc data/
    // -------------------------------------------------------------
    std::cout << "\n[TEST 1] Nap cau hinh tu data/ (rules, terrain, units)..." << std::endl;
    sim::SimConfig config;
    if (!config.autoLoad()) {
        std::cerr << "-> THAT BAI: Khong the tu dong nap cau hinh tu data/!" << std::endl;
        return 1;
    }

    assert(config.getRules().metersPerTile == 100);
    assert(config.getRules().tickPerSecond == 20);
    assert(config.getRules().timeScale == 10);

    assert(config.getTerrain().plainMultiplier == 100);
    assert(config.getTerrain().forestMultiplier == 60);
    assert(config.getTerrain().hillMultiplier == 50);
    assert(config.getTerrain().mountainMultiplier == 25);
    assert(config.getTerrain().riverMultiplier == 20);
    assert(config.getTerrain().seaMultiplier == 0);
    assert(config.getTerrain().lakeMultiplier == 0);

    assert(config.getInfantry().speedKmh == 5);
    assert(config.getInfantry().manpower == 3000);

    std::cout << "-> TEST 1 THANH CONG: Du lieu JSON nạp dung thong so yeu cau!" << std::endl;

    // -------------------------------------------------------------
    // Test 2: Kiem tra toc do dat bang va rung lech dung ti le 100:60
    // -------------------------------------------------------------
    std::cout << "\n[TEST 2] Kiem tra toc do tren dat bang va tren rung lech dung ti le 100:60..." << std::endl;

    // A. Kiem tra theo ham calculateDistanceMilli voi cac khoang thoi gian khac nhau
    for (int ticks : {72, 144, 360, 720, 1440}) {
        int64_t distPlain = config.calculateDistanceMilli(ticks, sim::TerrainType::Plain);
        int64_t distForest = config.calculateDistanceMilli(ticks, sim::TerrainType::Forest);

        std::cout << "  Ticks: " << ticks
                  << " | Quãng đường Đất bằng: " << distPlain << " mili-ô"
                  << " | Quãng đường Rừng: " << distForest << " mili-ô"
                  << " | Tỉ lệ: " << (distPlain * 60) << " == " << (distForest * 100) << std::endl;

        if (distPlain * 60 != distForest * 100) {
            std::cerr << "-> THAT BAI tai ticks=" << ticks << ": ti le khong dung 100:60!" << std::endl;
            return 1;
        }
    }

    // B. Kiem tra thuc te qua WorldSim (chay tick tung buoc mo phong voi Unit that)
    {
        sim::WorldMap map(128, 96, 32);
        // Dat hang 1 la Dat bang (Plain), hang 2 la Rung (Forest)
        for (int x = 0; x < 128; ++x) {
            map.setTile(x, 1, sim::TerrainType::Plain);
            map.setTile(x, 2, sim::TerrainType::Forest);
        }

        sim::WorldSim sim(map);
        sim.loadConfig();

        // Tao don vi 1 tren dat bang, don vi 2 tren rung
        sim.getUnits().clear();
        int32_t startX = 10 * 1000 + 500;
        int32_t startY_plain = 1 * 1000 + 500;
        int32_t startY_forest = 2 * 1000 + 500;

        sim.getUnits().emplace_back(1, sim::Faction::Player, sim::UnitType::InfantryDivision, startX, startY_plain, 3000);
        sim.getUnits().emplace_back(2, sim::Faction::Player, sim::UnitType::InfantryDivision, startX, startY_forest, 3000);

        // Ra lenh di chuyen ngang 80 o
        sim.queueMoveCommand({1}, 90, 1);
        sim.queueMoveCommand({2}, 90, 2);

        // Chay mo phong 72 ticks (3.6 giay)
        for (int t = 0; t < 72; ++t) {
            sim.tick();
        }

        const auto* uPlain = sim.getUnitById(1);
        const auto* uForest = sim.getUnitById(2);

        int32_t movedPlain = uPlain->getPosX() - startX;
        int32_t movedForest = uForest->getPosX() - startX;

        std::cout << "  [Thuc te Sim sau 72 tick] Dat bang di duoc: " << movedPlain
                  << " mili-o, Rung di duoc: " << movedForest << " mili-o" << std::endl;

        if (movedPlain * 60 != movedForest * 100) {
            std::cerr << "-> THAT BAI: Ty le di chuyen thuc te lech ("
                      << movedPlain * 60 << " != " << movedForest * 100 << ")" << std::endl;
            return 1;
        }

        if (movedPlain != 500 || movedForest != 300) {
            std::cerr << "-> THAT BAI: Gia tri quang duong ky vong 500 va 300 nhung nhan duoc: "
                      << movedPlain << " va " << movedForest << std::endl;
            return 1;
        }
    }
    std::cout << "-> TEST 2 THANH CONG: Ty le toc do tren dat bang va rung dat chuan 100:60 tuyet doi!" << std::endl;

    // -------------------------------------------------------------
    // Test 3: Kiem tra khoi tao 10 su doan tat dinh tu seed
    // -------------------------------------------------------------
    std::cout << "\n[TEST 3] Kiem tra sinh 10 su doan tat dinh tu seed tren dat bang..." << std::endl;
    {
        sim::WorldMap mapA(128, 96, 32);
        sim::WorldMap mapB(128, 96, 32);
        // Dien toan bo bang Plain ngoai tru vien
        mapA.fill(sim::TerrainType::Plain);
        mapB.fill(sim::TerrainType::Plain);

        sim::WorldSim simA(mapA);
        sim::WorldSim simB(mapB);
        simA.loadConfig();
        simB.loadConfig();

        uint64_t testSeed = 987654321ULL;
        simA.spawnPlayerDivisions(testSeed);
        simB.spawnPlayerDivisions(testSeed);

        assert(simA.getUnits().size() == 10);
        assert(simB.getUnits().size() == 10);

        for (size_t i = 0; i < 10; ++i) {
            const auto& ua = simA.getUnits()[i];
            const auto& ub = simB.getUnits()[i];
            if (ua.getPosX() != ub.getPosX() || ua.getPosY() != ub.getPosY()) {
                std::cerr << "-> THAT BAI: Don vi " << i << " vi tri khong giong nhau giua 2 lan chay!" << std::endl;
                return 1;
            }
            if (mapA.getTile(ua.getTileX(), ua.getTileY()) != sim::TerrainType::Plain) {
                std::cerr << "-> THAT BAI: Don vi " << i << " khong nam tren dat bang Plain!" << std::endl;
                return 1;
            }
            assert(ua.getManpower() == 3000);
            assert(ua.getFaction() == sim::Faction::Player);
        }
    }
    std::cout << "-> TEST 3 THANH CONG: 10 su doan khoi tao tat dinh 100% tren dat bang!" << std::endl;

    // -------------------------------------------------------------
    // Test 4: Di chuyen thang toi dich va dung lai khi den noi
    // -------------------------------------------------------------
    std::cout << "\n[TEST 4] Kiem tra di chuyen thang toi dich va den noi..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain);
        sim::WorldSim sim(map);
        sim.loadConfig();

        sim.getUnits().emplace_back(1, sim::Faction::Player, sim::UnitType::InfantryDivision, 10000, 10000, 3000);
        // Ra lenh di chuyen toi o (11, 10) -> destX = 11500, destY = 10500
        sim.queueMoveCommand({1}, 11, 10);

        // Khi chua tick, lenh van trong hang doi
        assert(sim.getUnits()[0].hasDestination() == false);

        // Tick 1: lenh duoc ap dung
        sim.tick();
        assert(sim.getUnits()[0].hasDestination() == true);
        assert(sim.getUnits()[0].getDestX() == 11500);
        assert(sim.getUnits()[0].getDestY() == 10500);

        // Chay du so tick de don vi toi dich (khoang cach khoang 1581 mili-o, moi tick ~6.94 mili-o -> ~230 tick)
        for (int t = 0; t < 300; ++t) {
            sim.tick();
        }

        const auto* u = sim.getUnitById(1);
        if (u->hasDestination()) {
            std::cerr << "-> THAT BAI: Don vi chua den dich sau 300 tick!" << std::endl;
            return 1;
        }
        if (u->getPosX() != 11500 || u->getPosY() != 10500) {
            std::cerr << "-> THAT BAI: Toa do cuoi khong trung tam o dich (11500, 10500)! Thuc te: ("
                      << u->getPosX() << ", " << u->getPosY() << ")" << std::endl;
            return 1;
        }
    }
    std::cout << "-> TEST 4 THANH CONG: Di chuyen thang toi dich va dung chinh xac!" << std::endl;

    std::cout << "\n==================================================\n";
    std::cout << " ALL TESTS PASSED! MOC M3a HOAN TOAN CHUAN XAC!    \n";
    std::cout << "==================================================\n";
    return 0;
}
