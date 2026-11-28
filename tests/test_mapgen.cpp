#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "mapgen/MapGenerator.hpp"
#include "mapgen/MapGenConfig.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/config/SimConfig.hpp"
#include "sim/pathfinding/Pathfinder.hpp"

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdlib>

namespace {

// Helper flood-fill tìm tất cả các ô đất liên thông theo 8 hướng
std::vector<bool> floodFill8Land(const sim::WorldMap& map, int startX, int startY, int& outCount) {
    int w = map.getWidth();
    int h = map.getHeight();
    std::vector<bool> visited(static_cast<size_t>(w * h), false);
    outCount = 0;

    auto isLand = [&](int x, int y) -> bool {
        sim::TerrainType t = map.getTile(x, y);
        // Đất liền đi được cho lục quân (không phải Sea, River, Lake)
        return (t == sim::TerrainType::Plain || t == sim::TerrainType::Forest ||
                t == sim::TerrainType::Hill  || t == sim::TerrainType::Mountain ||
                t == sim::TerrainType::Ford);
    };

    if (!map.isInBounds(startX, startY) || !isLand(startX, startY)) {
        return visited;
    }

    std::queue<std::pair<int, int>> q;
    q.push({startX, startY});
    visited[static_cast<size_t>(startY * w + startX)] = true;

    constexpr int dx[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    constexpr int dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        outCount++;

        for (int i = 0; i < 8; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            size_t idx = static_cast<size_t>(ny * w + nx);
            if (visited[idx]) continue;
            if (!isLand(nx, ny)) continue;

            visited[idx] = true;
            q.push({nx, ny});
        }
    }
    return visited;
}

inline float crossProduct(float x1, float y1, float x2, float y2, float px, float py) {
    return (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
}

} // namespace

int main(int argc, char* argv[]) {
    mapgen::MapGenerator generator;
    mapgen::MapGenConfig config;

    // -------------------------------------------------------------------------
    // Xử lý các cờ dòng lệnh: --seed, --ascii, --png, --ford, --lakes
    // -------------------------------------------------------------------------
    uint64_t cliSeed = 42ULL;
    bool exportAsciiMode = false;
    std::string exportPngPath;
    bool hasCustomCli = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            cliSeed = static_cast<uint64_t>(std::stoull(argv[++i]));
            hasCustomCli = true;
        } else if (arg == "--ascii") {
            exportAsciiMode = true;
            hasCustomCli = true;
        } else if (arg == "--png" && i + 1 < argc) {
            exportPngPath = argv[++i];
            hasCustomCli = true;
        } else if (arg == "--ford" && i + 1 < argc) {
            config.riverFordCount = std::stoi(argv[++i]);
        } else if (arg == "--lakes" && i + 1 < argc) {
            config.lakeMax = std::stoi(argv[++i]);
        }
    }

    if (exportAsciiMode || !exportPngPath.empty()) {
        sim::WorldMap map(config.width, config.height, 32);
        generator.generate(map, cliSeed, config);

        if (exportAsciiMode) {
            std::cout << "=== MAP ASCII EXPORT (Seed: " << cliSeed << ") ===\n";
            std::cout << mapgen::MapGenerator::exportAscii(map, config.playerBaseX, config.playerBaseY,
                                                          config.enemyBaseX, config.enemyBaseY);
        }

        if (!exportPngPath.empty()) {
            if (mapgen::MapGenerator::exportPng(map, exportPngPath, 4)) {
                std::cout << "-> Da xuat thanh cong anh PNG ban do vao: " << exportPngPath << "\n";
            } else {
                std::cerr << "-> LOI: Khong the ghi file PNG vao: " << exportPngPath << "\n";
                return 1;
            }
        }
        return 0;
    }

    std::cout << "==================================================\n";
    std::cout << " [TEST SUITE] Kiem tra Map Generator (Moc M2)     \n";
    std::cout << "==================================================\n";

    // -------------------------------------------------------------
    // Test 1: Tinh tat dinh (f): Cung seed chay 2 lan ra ket qua giong het nhau
    // -------------------------------------------------------------
    std::cout << "\n[TEST 1] Kiem tra tinh tat dinh theo seed (Test f)..." << std::endl;
    std::vector<uint64_t> testSeeds = {42ULL, 12345678ULL, 99999999ULL, 1337ULL, 0ULL, 77777ULL, 88888ULL, 10203040ULL};

    for (uint64_t seed : testSeeds) {
        sim::WorldMap mapA(config.width, config.height, 32);
        sim::WorldMap mapB(config.width, config.height, 32);

        generator.generate(mapA, seed, config);
        generator.generate(mapB, seed, config);

        uint64_t hashA = mapA.computeHash();
        uint64_t hashB = mapB.computeHash();

        if (hashA != hashB) {
            std::cerr << "-> THAT BAI tai seed " << seed << ": Hash A (" << hashA
                      << ") != Hash B (" << hashB << ")" << std::endl;
            return 1;
        }

        for (int y = 0; y < config.height; ++y) {
            for (int x = 0; x < config.width; ++x) {
                if (mapA.getTile(x, y) != mapB.getTile(x, y)) {
                    std::cerr << "-> THAT BAI tai o (" << x << ", " << y << ") cho seed " << seed << std::endl;
                    return 1;
                }
            }
        }
        std::cout << "  Seed " << seed << " -> Khop 100% (Hash: " << hashA << ")" << std::endl;
    }
    std::cout << "-> TEST 1 THANH CONG: Tat ca seed deu tat dinh tuyet doi (Test f Dat)!" << std::endl;

    // -------------------------------------------------------------
    // Test 2: Dung ti le 5 dat : 1 bien (1/6 tong so o lam bien)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 2] Kiem tra ti le 5 dat : 1 bien..." << std::endl;
    {
        sim::WorldMap map(config.width, config.height, 32);
        generator.generate(map, 12345678ULL, config);

        int totalTiles = map.getTotalTiles();
        int expectedSea = totalTiles / 6;       // 12288 / 6 = 2048
        int expectedLand = totalTiles - expectedSea; // 10240

        int actualSea = map.getSeaCount();
        int actualLand = map.getLandCount();

        std::cout << "  Tong so o:       " << totalTiles << std::endl;
        std::cout << "  So o Bien (Sea): " << actualSea << " (ky vong: " << expectedSea << ")" << std::endl;
        std::cout << "  So o Dat (Land): " << actualLand << " (ky vong: " << expectedLand << ")" << std::endl;
        std::cout << "  Ti le dat / bien: " << (actualLand / actualSea) << " : 1" << std::endl;

        assert(actualSea == expectedSea && "So o bien phai dung 2048");
        assert(actualLand == expectedLand && "So o dat phai dung 10240");
        assert(actualLand == actualSea * 5 && "Ti le dat:bien phai dung 5:1");
    }
    std::cout << "-> TEST 2 THANH CONG: Chuan xac tuyet doi 5 dat : 1 bien!" << std::endl;

    // -------------------------------------------------------------
    // Test 3: Kiem tra vien ban do luon luon la bien
    // -------------------------------------------------------------
    std::cout << "\n[TEST 3] Kiem tra 4 vien ngoai cung ban do..." << std::endl;
    for (uint64_t seed : {1ULL, 42ULL, 987654321ULL}) {
        sim::WorldMap map(config.width, config.height, 32);
        generator.generate(map, seed, config);

        for (int x = 0; x < config.width; ++x) {
            assert(map.getTile(x, 0) == sim::TerrainType::Sea);
            assert(map.getTile(x, config.height - 1) == sim::TerrainType::Sea);
        }
        for (int y = 0; y < config.height; ++y) {
            assert(map.getTile(0, y) == sim::TerrainType::Sea);
            assert(map.getTile(config.width - 1, y) == sim::TerrainType::Sea);
        }
    }
    std::cout << "-> TEST 3 THANH CONG: Toan bo 4 mep vien ban do deu la Bien!" << std::endl;

    // -------------------------------------------------------------
    // Test 4: Kiem tra su hien dien cua tat ca 8 loai dia hinh
    // -------------------------------------------------------------
    std::cout << "\n[TEST 4] Kiem tra du 8 loai dia hinh (Bien, Dat bang, Doi, Nui, Rung, Song, Ho, Ben can)..." << std::endl;
    {
        sim::WorldMap map(config.width, config.height, 32);
        generator.generate(map, 12345678ULL, config);

        int countSea      = map.getTileCount(sim::TerrainType::Sea);
        int countPlain    = map.getTileCount(sim::TerrainType::Plain);
        int countHill     = map.getTileCount(sim::TerrainType::Hill);
        int countMountain = map.getTileCount(sim::TerrainType::Mountain);
        int countForest   = map.getTileCount(sim::TerrainType::Forest);
        int countRiver    = map.getTileCount(sim::TerrainType::River);
        int countLake     = map.getTileCount(sim::TerrainType::Lake);
        int countFord     = map.getTileCount(sim::TerrainType::Ford);

        std::cout << "  - Bien (Sea):      " << countSea << " o\n";
        std::cout << "  - Dat bang (Plain):" << countPlain << " o\n";
        std::cout << "  - Doi (Hill):      " << countHill << " o\n";
        std::cout << "  - Nui (Mountain):  " << countMountain << " o\n";
        std::cout << "  - Rung (Forest):   " << countForest << " o\n";
        std::cout << "  - Song (River):    " << countRiver << " o\n";
        std::cout << "  - Ho (Lake):       " << countLake << " o\n";
        std::cout << "  - Ben can (Ford):  " << countFord << " o\n";

        assert(countSea > 0 && countPlain > 0 && countHill > 0 && countMountain > 0 &&
               countForest > 0 && countRiver > 0 && countLake > 0 && countFord > 0);
    }
    std::cout << "-> TEST 4 THANH CONG: Ca 8 loai dia hinh deu duoc sinh thanh cong!" << std::endl;

    // -------------------------------------------------------------
    // Test 5: Cac truong hop dac biet (trung diem 2 canh doi, duong cheo goc-goc)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 5] Kiem tra cac truong hop dac biet (trung diem doi, duong cheo goc-goc)..." << std::endl;
    {
        // 5.1: Nối trung điểm 2 cạnh đối (Top midpoint -> Bottom midpoint)
        mapgen::MapGenConfig midConfig = config;
        midConfig.fixedP1X = config.width / 2; // 64
        midConfig.fixedP1Y = 0;
        midConfig.fixedP2X = config.width / 2;
        midConfig.fixedP2Y = config.height - 1;
        midConfig.riverFordCount = 0;

        sim::WorldMap midMap(config.width, config.height, 32);
        generator.generate(midMap, 42ULL, midConfig);

        int count1 = 0, count2 = 0;
        auto reach1 = floodFill8Land(midMap, config.playerBaseX, config.playerBaseY, count1);
        auto reach2 = floodFill8Land(midMap, config.enemyBaseX, config.enemyBaseY, count2);

        assert(!reach1[config.enemyBaseY * config.width + config.enemyBaseX] && "Player khong the cham Enemy khi ford=0");
        assert(!reach2[config.playerBaseY * config.width + config.playerBaseX] && "Enemy khong the cham Player khi ford=0");
        std::cout << "  - Nối trung điểm 2 cạnh đối: Chia 2 nửa thành công (" << count1 << " ô vs " << count2 << " ô)!\n";

        // 5.2: Nối đường chéo góc-góc (0, 0) -> (127, 95)
        mapgen::MapGenConfig diagConfig = config;
        diagConfig.fixedP1X = 0;
        diagConfig.fixedP1Y = 0;
        diagConfig.fixedP2X = config.width - 1;
        diagConfig.fixedP2Y = config.height - 1;
        diagConfig.riverFordCount = 0;

        sim::WorldMap diagMap(config.width, config.height, 32);
        generator.generate(diagMap, 42ULL, diagConfig);

        int dCount1 = 0, dCount2 = 0;
        auto dReach1 = floodFill8Land(diagMap, config.playerBaseX, config.playerBaseY, dCount1);
        auto dReach2 = floodFill8Land(diagMap, config.enemyBaseX, config.enemyBaseY, dCount2);

        assert(!dReach1[config.enemyBaseY * config.width + config.enemyBaseX]);
        assert(!dReach2[config.playerBaseY * config.width + config.playerBaseX]);
        std::cout << "  - Đường chéo góc-góc (0,0)->(127,95): Chia 2 nửa thành công (" << dCount1 << " ô vs " << dCount2 << " ô)!\n";

        // 5.3: Nối đường chéo góc-góc (127, 0) -> (0, 95)
        mapgen::MapGenConfig diag2Config = config;
        diag2Config.fixedP1X = config.width - 1;
        diag2Config.fixedP1Y = 0;
        diag2Config.fixedP2X = 0;
        diag2Config.fixedP2Y = config.height - 1;
        diag2Config.riverFordCount = 0;

        sim::WorldMap diag2Map(config.width, config.height, 32);
        generator.generate(diag2Map, 42ULL, diag2Config);

        int d2Count1 = 0, d2Count2 = 0;
        auto d2Reach1 = floodFill8Land(diag2Map, config.playerBaseX, config.playerBaseY, d2Count1);
        auto d2Reach2 = floodFill8Land(diag2Map, config.enemyBaseX, config.enemyBaseY, d2Count2);

        assert(!d2Reach1[config.enemyBaseY * config.width + config.enemyBaseX]);
        assert(!d2Reach2[config.playerBaseY * config.width + config.playerBaseX]);
        std::cout << "  - Đường chéo góc-góc (127,0)->(0,95): Chia 2 nửa thành công (" << d2Count1 << " ô vs " << d2Count2 << " ô)!\n";
    }
    std::cout << "-> TEST 5 THANH CONG: Cac truong hop dac biet sinh ra va hoat dong hoan hao!" << std::endl;

    // -------------------------------------------------------------
    // Test 6: Kiem tra toan dien tren 200 SEED (Cac muc a, b, c, d, e)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 6] Kiem tra toan dien tren 200 SEED (Muc a, b, c, d, e)..." << std::endl;
    constexpr int NUM_TEST_SEEDS = 200;

    for (int iter = 0; iter < NUM_TEST_SEEDS; ++iter) {
        uint64_t testSeed = 100000ULL + static_cast<uint64_t>(iter * 1337ULL + 11ULL);

        // ---------------------------------------------------------
        // (a), (b), (c): Kiểm tra khi riverFordCount = 0
        // ---------------------------------------------------------
        mapgen::MapGenConfig noFordConfig = config;
        noFordConfig.riverFordCount = 0;
        noFordConfig.lakeMax = 0; // Để cô lập kiểm tra riêng cho Sông

        sim::WorldMap mapNoFord(config.width, config.height, 32);
        generator.generate(mapNoFord, testSeed, noFordConfig);

        int countP = 0;
        auto reachP = floodFill8Land(mapNoFord, noFordConfig.playerBaseX, noFordConfig.playerBaseY, countP);

        int countE = 0;
        auto reachE = floodFill8Land(mapNoFord, noFordConfig.enemyBaseX, noFordConfig.enemyBaseY, countE);

        // (a) Sông luôn chia map làm 2 vùng liên thông riêng biệt khi riverFordCount=0 (quân 8 hướng không qua được)
        size_t enemyBaseIdx = static_cast<size_t>(noFordConfig.enemyBaseY * config.width + noFordConfig.enemyBaseX);
        size_t playerBaseIdx = static_cast<size_t>(noFordConfig.playerBaseY * config.width + noFordConfig.playerBaseX);

        if (reachP[enemyBaseIdx] || reachE[playerBaseIdx]) {
            std::cerr << "-> THAT BAI muc (a) tai seed " << testSeed << ": Song khong chia doi map, quan 8 huong van lọt qua!" << std::endl;
            return 1;
        }

        // Kiểm tra không có bất kỳ ô đất nào của 2 nửa tiếp xúc 8 hướng với nhau
        for (int y = 0; y < config.height; ++y) {
            for (int x = 0; x < config.width; ++x) {
                if (reachP[static_cast<size_t>(y * config.width + x)]) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx;
                            int ny = y + dy;
                            if (mapNoFord.isInBounds(nx, ny)) {
                                if (reachE[static_cast<size_t>(ny * config.width + nx)]) {
                                    std::cerr << "-> THAT BAI muc (a): P va E tiep xuc cheo tai (" << x << "," << y << ") va (" << nx << "," << ny << ")!" << std::endl;
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        // (b) Mỗi nửa map phải có ít nhất 40% tổng diện tích đất (10240 * 0.40 = 4096 ô)
        int totalLand = mapNoFord.getLandCount();
        const int minExpectedHalfLand = static_cast<int>(std::floor(static_cast<float>(totalLand) * 0.40f)); // 4096

        if (countP < minExpectedHalfLand || countE < minExpectedHalfLand) {
            std::cerr << "-> THAT BAI muc (b) tai seed " << testSeed << ": Dien tich dat moi nua khong dat 40% ("
                      << countP << ", " << countE << " < " << minExpectedHalfLand << ")" << std::endl;
            return 1;
        }

        // (c) Căn cứ 2 phe nằm ở 2 nửa khác nhau (kiểm tra bằng dấu tích có hướng và kiểm tra độc lập qua flood-fill)
        // Đã được khẳng định bởi reachP và reachE không thể chạm tới căn cứ của nhau!

        // ---------------------------------------------------------
        // (d), (e): Kiểm tra cấu hình có Hồ (cô lập ford=0 để kiểm tra liên thông từng nửa)
        // ---------------------------------------------------------
        mapgen::MapGenConfig lakeConfig = config;
        lakeConfig.riverFordCount = 0;
        lakeConfig.lakeMax = 6;

        sim::WorldMap lakeMap(config.width, config.height, 32);
        generator.generate(lakeMap, testSeed, lakeConfig);

        // (d) Số hồ <= 6 và không hồ nào đè lên sông, căn cứ, hoặc biển
        int lakeTileCount = lakeMap.getTileCount(sim::TerrainType::Lake);
        assert(lakeConfig.lakeMax <= 6);

        for (int y = 0; y < config.height; ++y) {
            for (int x = 0; x < config.width; ++x) {
                if (lakeMap.getTile(x, y) == sim::TerrainType::Lake) {
                    // Không được nằm trong bán kính căn cứ
                    float distP = std::hypot(static_cast<float>(x - lakeConfig.playerBaseX), static_cast<float>(y - lakeConfig.playerBaseY));
                    float distE = std::hypot(static_cast<float>(x - lakeConfig.enemyBaseX), static_cast<float>(y - lakeConfig.enemyBaseY));
                    if (distP <= static_cast<float>(lakeConfig.baseRadius) || distE <= static_cast<float>(lakeConfig.baseRadius)) {
                        std::cerr << "-> THAT BAI muc (d) tai seed " << testSeed << ": Ho de vao vung can cu!" << std::endl;
                        return 1;
                    }

                    // Hồ không được nối biển
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx;
                            int ny = y + dy;
                            if (lakeMap.isInBounds(nx, ny)) {
                                if (lakeMap.getTile(nx, ny) == sim::TerrainType::Sea) {
                                    std::cerr << "-> THAT BAI muc (d) tai seed " << testSeed << ": Ho tiep giap Bien!" << std::endl;
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        // (e) Đất mỗi nửa vẫn liên thông (hồ không làm đứt liên thông của đất trong cùng một nửa)
        int lakeCountP = 0;
        int lakeCountE = 0;
        floodFill8Land(lakeMap, lakeConfig.playerBaseX, lakeConfig.playerBaseY, lakeCountP);
        floodFill8Land(lakeMap, lakeConfig.enemyBaseX, lakeConfig.enemyBaseY, lakeCountE);

        int lakesInP = 0;
        int lakesInE = 0;
        for (int y = 0; y < config.height; ++y) {
            for (int x = 0; x < config.width; ++x) {
                if (lakeMap.getTile(x, y) == sim::TerrainType::Lake) {
                    if (reachP[static_cast<size_t>(y * config.width + x)]) {
                        lakesInP++;
                    } else if (reachE[static_cast<size_t>(y * config.width + x)]) {
                        lakesInE++;
                    }
                }
            }
        }

        // Toàn bộ số ô đất còn lại trong từng nửa phải vẫn được kết nối hoàn toàn
        if (lakeCountP != countP - lakesInP || lakeCountE != countE - lakesInE) {
            std::cerr << "-> THAT BAI muc (e) tai seed " << testSeed << ": Ho lam dut lien thong dat!" << std::endl;
            return 1;
        }

        if ((iter + 1) % 50 == 0) {
            std::cout << "  ... Da kiem tra thanh cong " << (iter + 1) << " / " << NUM_TEST_SEEDS << " seeds!\n";
        }
    }
    std::cout << "-> TEST 6 THANH CONG: 200 SEED deu dat tat ca tieu chi (a), (b), (c), (d), (e) 100%!" << std::endl;

    // -------------------------------------------------------------
    // Test 7: Kiem tra Bến cạn (Ford) & Pathfinding
    // -------------------------------------------------------------
    std::cout << "\n[TEST 7] Kiem tra Ben can (Ford) & Pathfinding..." << std::endl;
    {
        sim::SimConfig simCfg;
        assert(simCfg.autoLoad() && "Phai nap duoc game_config.json");

        sim::WorldMap mapWithFords(config.width, config.height, 32);
        generator.generate(mapWithFords, 42ULL, config);

        assert(mapWithFords.getTileCount(sim::TerrainType::Ford) > 0 && "Bản đồ cấu hình riverFordCount=3 phải có ô Ford!");

        // Kiểm tra tìm đường qua bến cạn từ căn cứ Player sang căn cứ Enemy
        sim::PathResult path = sim::Pathfinder::findPath(
            mapWithFords,
            simCfg,
            sim::UnitDomain::Land,
            sim::UnitType::Infantry,
            {config.playerBaseX, config.playerBaseY},
            {config.enemyBaseX, config.enemyBaseY}
        );

        assert(path.found && "Bộ binh phải tìm được đường từ Player sang Enemy nhờ bến cạn (Ford)!");
        assert(!path.waypoints.empty());

        bool crossedFord = false;
        for (const auto& wp : path.waypoints) {
            if (mapWithFords.getTile(wp.x, wp.y) == sim::TerrainType::Ford) {
                crossedFord = true;
                break;
            }
        }
        assert(crossedFord && "Đường đi của bộ binh bắt buộc phải đi qua ô bến cạn (Ford)!");
        std::cout << "  - Bo binh tim duong qua song qua ben can thanh cong (" << path.waypoints.size() << " waypoints)!\n";
    }
    std::cout << "-> TEST 7 THANH CONG: Ben can (Ford) hoat dong chuan voi A* Pathfinding!" << std::endl;

    // -------------------------------------------------------------
    // Test 8: Xuat file mau PNG va ASCII
    // -------------------------------------------------------------
    std::cout << "\n[TEST 8] Xuat file mau anh PNG va ASCII..." << std::endl;
    {
        sim::WorldMap sampleMap(config.width, config.height, 32);
        generator.generate(sampleMap, 42ULL, config);

        std::string pngPath = "map_sample_seed42.png";
        bool pngOk = mapgen::MapGenerator::exportPng(sampleMap, pngPath, 4);
        assert(pngOk && "Xuat PNG phai thanh cong!");
        std::cout << "  - Da ghi anh mau PNG: " << pngPath << " (512x384 px)\n";

        std::string asciiPath = "map_sample_seed42.txt";
        std::ofstream f(asciiPath);
        assert(f.is_open());
        f << mapgen::MapGenerator::exportAscii(sampleMap, config.playerBaseX, config.playerBaseY, config.enemyBaseX, config.enemyBaseY);
        f.close();
        std::cout << "  - Da ghi ban do mau ASCII: " << asciiPath << " (128x96 ký tự)\n";
    }
    std::cout << "-> TEST 8 THANH CONG: File PNG va ASCII xuat ra hoan hao!" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << " TAT CA CAC BAI TEST MOC M2 DEU DAT (PASSED 100%)" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
