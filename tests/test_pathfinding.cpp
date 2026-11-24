#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "sim/config/SimConfig.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/pathfinding/Pathfinder.hpp"

#include <iostream>
#include <vector>

int main() {
    std::cout << "==================================================\n";
    std::cout << " [TEST SUITE] Kiem tra Thuat toan Tim duong A* M4\n";
    std::cout << "==================================================\n";

    sim::SimConfig config;
    if (!config.autoLoad()) {
        std::cerr << "-> THAT BAI: Khong the nạp cau hinh tu data/game_config.json!" << std::endl;
        return 1;
    }

    // -------------------------------------------------------------
    // Test 1: Tranh o khong di duoc (Impassable Obstacle Avoidance)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 1] Kiem tra tranh o khong di duoc (buc tuong dia hinh)..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain);

        // Dat mot buc tuong bien (Sea) chan o x = 10, y tu 5 den 15
        // Voi xe tang Leopard 2A, Sea la impassable
        for (int y = 5; y <= 15; ++y) {
            map.setTile(10, y, sim::TerrainType::Sea);
        }

        sim::TileCoord start{5, 10};
        sim::TileCoord target{15, 10};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Land, sim::UnitType::Leopard2A, start, target
        );

        assert(path.found == true);
        assert(path.adjusted == false);
        assert(path.finalTarget == target);
        assert(!path.waypoints.empty());
        assert(path.waypoints.back() == target);

        // Kiem tra khong co bat ky waypoint nao di vao buc tuong Sea
        for (const auto& wp : path.waypoints) {
            sim::DetailedTerrainType dt = config.classifyTerrain(map, wp.x, wp.y);
            assert(config.isPassable(dt, sim::UnitDomain::Land, sim::UnitType::Leopard2A));
            assert(!(wp.x == 10 && wp.y >= 5 && wp.y <= 15));
        }
    }
    std::cout << "-> TEST 1 THANH CONG: A* di vong quanh buc tuong, khong di vao o khong the di!" << std::endl;

    // -------------------------------------------------------------
    // Test 2: Chon duong vong khi re hon di qua dia hinh cham
    // -------------------------------------------------------------
    std::cout << "\n[TEST 2] Kiem tra chon duong vong khi re hon di qua dia hinh cham..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain); // Plain speed = 1.0 (cost = 1000)

        // Dat mot dai Nui (Mountain) o x = 10..14, y = 8..12
        // Voi bo binh (Infantry), Mountain speed = 0.5 (cost = 2000, gap doi Plain)
        for (int y = 8; y <= 12; ++y) {
            for (int x = 10; x <= 14; ++x) {
                map.setTile(x, y, sim::TerrainType::Mountain);
            }
        }

        sim::TileCoord start{8, 10};
        sim::TileCoord target{16, 10};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Land, sim::UnitType::Infantry, start, target
        );

        assert(path.found == true);
        assert(!path.waypoints.empty());

        // Kiem tra duong di lua chon vong qua Plain (y < 8 hoac y > 12) thay vi dam xuyen qua Nui
        bool traversedMountain = false;
        for (const auto& wp : path.waypoints) {
            if (wp.x >= 10 && wp.x <= 14 && wp.y >= 8 && wp.y <= 12) {
                traversedMountain = true;
                break;
            }
        }
        // Duong vong quanh nui re hon (vi di xuyen nui ton 5 o * 2000 = 10000, trong khi di vong chi ton ~6 o Plain = 6000-8000)
        assert(traversedMountain == false);
    }
    std::cout << "-> TEST 2 THANH CONG: A* thong minh chon duong vong Plain re hon di xuyen vung Nui cham!" << std::endl;

    // -------------------------------------------------------------
    // Test 3: Tau chi o tren bien / ho (DEEP_SEA, COASTAL_SEA)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 3] Kiem tra tau chi di chuyen tren bien/ho..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Sea);

        // Dat mot ban dao dat lien (Plain) o giua
        for (int y = 0; y <= 50; ++y) {
            for (int x = 20; x <= 40; ++x) {
                map.setTile(x, y, sim::TerrainType::Plain);
            }
        }

        sim::TileCoord start{10, 20};
        sim::TileCoord target{50, 20};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Sea, sim::UnitType::Mogami, start, target
        );

        assert(path.found == true);
        assert(!path.waypoints.empty());
        assert(path.waypoints.back() == target);

        // Kiem tra toan bo waypoint cua tau chi o tren bien
        for (const auto& wp : path.waypoints) {
            sim::DetailedTerrainType dt = config.classifyTerrain(map, wp.x, wp.y);
            assert(dt == sim::DetailedTerrainType::DeepSea || dt == sim::DetailedTerrainType::CoastalSea);
            assert(config.isPassable(dt, sim::UnitDomain::Sea, sim::UnitType::Mogami));
            // Khong duoc di qua ban dao dat lien x=20..40, y=0..50
            assert(!(wp.x >= 20 && wp.x <= 40 && wp.y <= 50));
        }
    }
    std::cout << "-> TEST 3 THANH CONG: Tau chi di chuyen tren bien/ho, khong bao gio len bo!" << std::endl;

    // -------------------------------------------------------------
    // Test 4: May bay bay thang bo qua moi dia hinh
    // -------------------------------------------------------------
    std::cout << "\n[TEST 4] Kiem tra may bay bay thang bo qua dia hinh..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        // Ban do lon xon du loai dia hinh tu nui cao den bien sau
        map.fill(sim::TerrainType::Mountain);

        sim::TileCoord start{5, 5};
        sim::TileCoord target{80, 60};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Air, sim::UnitType::Su57, start, target
        );

        assert(path.found == true);
        assert(path.adjusted == false);
        assert(path.finalTarget == target);
        // May bay chi can 1 waypoint duy nhat bay thang den dich
        assert(path.waypoints.size() == 1);
        assert(path.waypoints[0] == target);
    }
    std::cout << "-> TEST 4 THANH CONG: May bay bay thang toi dich bo qua dia hinh!" << std::endl;

    // -------------------------------------------------------------
    // Test 5: Dich khong den duoc -> dua ve o gan dich nhat va bat co adjusted
    // -------------------------------------------------------------
    std::cout << "\n[TEST 5] Kiem tra dich khong den duoc thi dua ve o gan dich nhat..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        // Nua phia Tay la Bien (Sea), nua phia Dong la Dat (Plain)
        map.fill(sim::TerrainType::Sea);
        for (int y = 0; y < 96; ++y) {
            for (int x = 60; x < 128; ++x) {
                map.setTile(x, y, sim::TerrainType::Plain);
            }
        }

        // Tau Mogami o bien (10, 20), nhan lenh den (100, 20) nam sau trong dat lien
        sim::TileCoord start{10, 20};
        sim::TileCoord unreachableTarget{100, 20};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Sea, sim::UnitType::Mogami, start, unreachableTarget
        );

        assert(path.found == true);
        assert(path.adjusted == true); // Phai bat co dieu chinh dich
        assert(path.originalTarget == unreachableTarget);
        // Dich moi phai la mot o bien gan nhat (sat bo bien x=59, y=20)
        assert(path.finalTarget.x == 59 && path.finalTarget.y == 20);
        sim::DetailedTerrainType dtFinal = config.classifyTerrain(map, path.finalTarget.x, path.finalTarget.y);
        assert(config.isPassable(dtFinal, sim::UnitDomain::Sea, sim::UnitType::Mogami));
    }
    std::cout << "-> TEST 5 THANH CONG: Dich khong den duoc da tu dong dieu chinh ve o gan nhat va bat co adjusted!" << std::endl;

    // -------------------------------------------------------------
    // Test 6: Tinh tat dinh (Deterministic)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 6] Kiem tra tinh tat dinh..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain);
        for (int i = 10; i < 30; ++i) {
            map.setTile(25, i, sim::TerrainType::Mountain);
            map.setTile(i, 20, sim::TerrainType::River);
        }

        sim::TileCoord start{5, 5};
        sim::TileCoord target{40, 35};

        sim::PathResult pathA = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Land, sim::UnitType::Puma, start, target
        );
        sim::PathResult pathB = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Land, sim::UnitType::Puma, start, target
        );

        assert(pathA.found == pathB.found);
        assert(pathA.adjusted == pathB.adjusted);
        assert(pathA.finalTarget == pathB.finalTarget);
        assert(pathA.waypoints.size() == pathB.waypoints.size());

        for (size_t i = 0; i < pathA.waypoints.size(); ++i) {
            assert(pathA.waypoints[i] == pathB.waypoints[i]);
        }
    }
    std::cout << "-> TEST 6 THANH CONG: Thuat toan tim duong tat dinh 100%!" << std::endl;

    // -------------------------------------------------------------
    // Test 7: Luat cam cat goc qua 2 o cheo bi chan (Corner Cutting Avoidance)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 7] Kiem tra cam cat goc qua 2 o cheo bi chan..." << std::endl;
    {
        sim::WorldMap map(128, 96, 32);
        map.fill(sim::TerrainType::Plain);

        // Tao 2 o bi chan o (10, 9) va (9, 10)
        // Di tu (9, 9) toi (10, 10) la di cheo cat ngang qua 2 o bi chan nay
        map.setTile(10, 9, sim::TerrainType::Sea);
        map.setTile(9, 10, sim::TerrainType::Sea);

        sim::TileCoord start{9, 9};
        sim::TileCoord target{10, 10};

        sim::PathResult path = sim::Pathfinder::findPath(
            map, config, sim::UnitDomain::Land, sim::UnitType::Infantry, start, target
        );

        assert(path.found == true);
        // Nuoc di dau tien tu (9, 9) khong duoc phep nhay thang sang (10, 10)
        // ma phai di vong (vi du: (8, 9) hoac (9, 8)...)
        assert(path.waypoints.front() != sim::TileCoord(10, 10));
    }
    std::cout << "-> TEST 7 THANH CONG: Thuat toan tuyet doi tuan thu luat cam cat goc qua hai o cheo bi chan!" << std::endl;

    std::cout << "\n==================================================\n";
    std::cout << " ALL TESTS PASSED! MOC M4 HOAN TOAN CHUAN XAC!    \n";
    std::cout << "==================================================\n";
    return 0;
}
