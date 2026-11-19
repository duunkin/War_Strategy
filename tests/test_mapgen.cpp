#include "mapgen/MapGenerator.hpp"
#include "mapgen/MapGenConfig.hpp"
#include "sim/map/WorldMap.hpp"

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdlib>

int main() {
    std::cout << "==================================================\n";
    std::cout << " [TEST SUITE] Kiem tra Map Generator (Moc M2)     \n";
    std::cout << "==================================================\n";

    mapgen::MapGenerator generator;
    mapgen::MapGenConfig config;

    // -------------------------------------------------------------
    // Test 1: Tinh tat dinh (cung seed chay 2 lan ra ket qua giong het nhau)
    // -------------------------------------------------------------
    std::cout << "\n[TEST 1] Kiem tra tinh tat dinh theo seed (Hash & Tile)..." << std::endl;
    std::vector<uint64_t> testSeeds = {42ULL, 12345678ULL, 99999999ULL, 1337ULL, 0ULL};

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

        // So sanh tung o mot de chac chan 100%
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
    std::cout << "-> TEST 1 THANH CONG: Tat ca seed deu tat dinh tuyet doi!" << std::endl;

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

        if (actualSea != expectedSea) {
            std::cerr << "-> THAT BAI: So o bien (" << actualSea << ") != ky vong (" << expectedSea << ")" << std::endl;
            return 1;
        }

        if (actualLand != expectedLand) {
            std::cerr << "-> THAT BAI: So o dat (" << actualLand << ") != ky vong (" << expectedLand << ")" << std::endl;
            return 1;
        }

        if (actualLand != actualSea * 5) {
            std::cerr << "-> THAT BAI: Ti le dat:bien khong phai 5:1!" << std::endl;
            return 1;
        }
    }
    std::cout << "-> TEST 2 THANH CONG: Chuan xac tuyet doi 5 dat : 1 bien!" << std::endl;

    // -------------------------------------------------------------
    // Test 3: Kiem tra vien ban do luon luon la bien
    // -------------------------------------------------------------
    std::cout << "\n[TEST 3] Kiem tra 4 vien ngoai cung ban do..." << std::endl;
    for (uint64_t seed : {1ULL, 42ULL, 987654321ULL}) {
        sim::WorldMap map(config.width, config.height, 32);
        generator.generate(map, seed, config);

        // Mép trên và dưới
        for (int x = 0; x < config.width; ++x) {
            if (map.getTile(x, 0) != sim::TerrainType::Sea) {
                std::cerr << "-> THAT BAI tai o bien tren (" << x << ", 0)" << std::endl;
                return 1;
            }
            if (map.getTile(x, config.height - 1) != sim::TerrainType::Sea) {
                std::cerr << "-> THAT BAI tai o bien duoi (" << x << ", " << config.height - 1 << ")" << std::endl;
                return 1;
            }
        }
        // Mép trái và phải
        for (int y = 0; y < config.height; ++y) {
            if (map.getTile(0, y) != sim::TerrainType::Sea) {
                std::cerr << "-> THAT BAI tai o bien trai (0, " << y << ")" << std::endl;
                return 1;
            }
            if (map.getTile(config.width - 1, y) != sim::TerrainType::Sea) {
                std::cerr << "-> THAT BAI tai o bien phai (" << config.width - 1 << ", " << y << ")" << std::endl;
                return 1;
            }
        }
    }
    std::cout << "-> TEST 3 THANH CONG: Toan bo 4 mep vien ban do deu la Bien!" << std::endl;

    // -------------------------------------------------------------
    // Test 4: Kiem tra su hien dien cua tat ca 7 loai dia hinh
    // -------------------------------------------------------------
    std::cout << "\n[TEST 4] Kiem tra du 7 loai dia hinh (Bien, Dat bang, Doi, Nui, Rung, Song, Ho)..." << std::endl;
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

        std::cout << "  - Bien (Sea):      " << countSea << " o\n";
        std::cout << "  - Dat bang (Plain):" << countPlain << " o\n";
        std::cout << "  - Doi (Hill):      " << countHill << " o\n";
        std::cout << "  - Nui (Mountain):  " << countMountain << " o\n";
        std::cout << "  - Rung (Forest):   " << countForest << " o\n";
        std::cout << "  - Song (River):    " << countRiver << " o\n";
        std::cout << "  - Ho (Lake):       " << countLake << " o\n";

        if (countSea == 0 || countPlain == 0 || countHill == 0 ||
            countMountain == 0 || countForest == 0 || countRiver == 0 || countLake == 0) {
            std::cerr << "-> THAT BAI: Co it nhat mot loai dia hinh bi thieu tren ban do!" << std::endl;
            return 1;
        }
    }
    std::cout << "-> TEST 4 THANH CONG: Ca 7 loai dia hinh deu duoc sinh thanh cong!" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << " TAT CA CAC BAI TEST MOC M2 DEU DAT (PASSED 100%)" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
