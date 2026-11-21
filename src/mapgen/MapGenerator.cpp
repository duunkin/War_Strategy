#include "MapGenerator.hpp"
#include "FastRng.hpp"
#include "SimplexNoise.hpp"
#include "sim/map/WorldMap.hpp"

#include <algorithm>
#include <vector>

namespace mapgen {

void MapGenerator::generate(sim::WorldMap& map, uint64_t seed, const MapGenConfig& config) const {
    const int width = config.width;
    const int height = config.height;
    const int totalTiles = width * height;

    map.resize(width, height, map.getTileSize());

    std::vector<int32_t> heights(static_cast<size_t>(totalTiles), 0);

    FastRng rng(seed);
    SimplexNoise elevNoise(seed);
    SimplexNoise forestNoise(seed + 101ULL);

    // =========================================================================
    // 1. Độ cao bằng Simplex noise nhiều tầng (octave).
    //    Viền bản đồ luôn là biển (giảm độ cao dần về phía mép).
    // =========================================================================
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            int32_t h = elevNoise.sampleOctaves(x, y, config.elevationOctaves, config.elevationFrequencyFP, config.elevationPersistenceFP);

            int dx = std::min(x, width - 1 - x);
            int dy = std::min(y, height - 1 - y);
            int distEdge = std::min(dx, dy);

            // Áp dụng đường cong bậc 2 kéo tụt độ cao tại các ô gần mép
            if (distEdge < config.borderFalloffDistance) {
                int diff = config.borderFalloffDistance - distEdge;
                h -= diff * diff * config.borderFalloffStrength;
            }
            heights[static_cast<size_t>(idx)] = h;
        }
    }

    // =========================================================================
    // 2. Tỉ lệ 5 đất : 1 biển: sắp xếp độ cao các ô và lấy 1/6 số ô thấp nhất làm biển.
    // =========================================================================
    std::vector<int> sortedIndices(static_cast<size_t>(totalTiles));
    for (int i = 0; i < totalTiles; ++i) {
        sortedIndices[static_cast<size_t>(i)] = i;
    }

    // Sắp xếp tất định 100%: nếu cùng độ cao thì so sánh chỉ số ô
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&heights](int a, int b) {
        if (heights[static_cast<size_t>(a)] != heights[static_cast<size_t>(b)]) {
            return heights[static_cast<size_t>(a)] < heights[static_cast<size_t>(b)];
        }
        return a < b;
    });

    const int seaCount = (totalTiles * config.seaRatioNumerator) / config.seaRatioDenominator; // 2048 ô
    const int landCount = totalTiles - seaCount; // 10240 ô

    // 1/6 số ô thấp nhất được gán làm Biển (Sea)
    for (int i = 0; i < seaCount; ++i) {
        map.setTileByIndex(sortedIndices[static_cast<size_t>(i)], sim::TerrainType::Sea);
    }

    // =========================================================================
    // 3. Phân loại đất liền: Núi, Đồi, Đất bằng theo các ngưỡng cấu hình
    // =========================================================================
    const int mountainCount = (landCount * config.mountainPermille) / 1000;
    const int hillCount = (landCount * config.hillPermille) / 1000;

    const int mountainStartIndex = totalTiles - mountainCount;
    for (int i = mountainStartIndex; i < totalTiles; ++i) {
        map.setTileByIndex(sortedIndices[static_cast<size_t>(i)], sim::TerrainType::Mountain);
    }

    const int hillStartIndex = mountainStartIndex - hillCount;
    for (int i = hillStartIndex; i < mountainStartIndex; ++i) {
        map.setTileByIndex(sortedIndices[static_cast<size_t>(i)], sim::TerrainType::Hill);
    }

    for (int i = seaCount; i < hillStartIndex; ++i) {
        map.setTileByIndex(sortedIndices[static_cast<size_t>(i)], sim::TerrainType::Plain);
    }

    // =========================================================================
    // 4. Rừng đặt theo noise thứ hai trên đất bằng
    // =========================================================================
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (map.getTileByIndex(idx) == sim::TerrainType::Plain) {
                int32_t fn = forestNoise.sampleOctaves(x, y, config.forestOctaves, config.forestFrequencyFP, 32768);
                if (fn > config.forestThresholdFP) {
                    map.setTileByIndex(idx, sim::TerrainType::Forest);
                }
            }
        }
    }

    // =========================================================================
    // 5. Hồ nước ngọt nội địa (Lake): đặt tại các thung lũng đất bằng trũng sâu
    // =========================================================================
    std::vector<int> inlandPlainCandidates;
    const int margin = config.lakeInlandMargin;
    for (int y = margin; y < height - margin; ++y) {
        for (int x = margin; x < width - margin; ++x) {
            int idx = y * width + x;
            if (map.getTileByIndex(idx) == sim::TerrainType::Plain) {
                inlandPlainCandidates.push_back(idx);
            }
        }
    }

    std::sort(inlandPlainCandidates.begin(), inlandPlainCandidates.end(), [&heights](int a, int b) {
        if (heights[static_cast<size_t>(a)] != heights[static_cast<size_t>(b)]) {
            return heights[static_cast<size_t>(a)] < heights[static_cast<size_t>(b)];
        }
        return a < b;
    });

    int lakesCreated = 0;
    for (size_t i = 0; i < inlandPlainCandidates.size() && lakesCreated < config.lakeCount; i += 25) {
        int cIdx = inlandPlainCandidates[i];
        int cx = cIdx % width;
        int cy = cIdx / width;
        map.setTileByIndex(cIdx, sim::TerrainType::Lake);

        static const int DXY[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < config.lakeExpansionNeighbors; ++d) {
            int nx = cx + DXY[d][0];
            int ny = cy + DXY[d][1];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nIdx = ny * width + nx;
                sim::TerrainType currentT = map.getTileByIndex(nIdx);
                if (currentT == sim::TerrainType::Plain || currentT == sim::TerrainType::Forest) {
                    map.setTileByIndex(nIdx, sim::TerrainType::Lake);
                }
            }
        }
        lakesCreated++;
    }

    // =========================================================================
    // 6. Sông bắt đầu từ vùng núi/đồi cao và chảy xuống thấp tới biển hoặc hồ
    // =========================================================================
    std::vector<int> highCandidates;
    const int rMargin = config.riverStartMargin;
    for (int i = 0; i < totalTiles; ++i) {
        sim::TerrainType t = map.getTileByIndex(i);
        if (t == sim::TerrainType::Mountain || t == sim::TerrainType::Hill) {
            int x = i % width;
            int y = i / width;
            if (x > rMargin && x < width - rMargin && y > rMargin && y < height - rMargin) {
                highCandidates.push_back(i);
            }
        }
    }

    // Xáo trộn tất định các điểm bắt đầu tiềm năng
    for (int i = static_cast<int>(highCandidates.size()) - 1; i > 0; --i) {
        int j = rng.range(0, i);
        std::swap(highCandidates[static_cast<size_t>(i)], highCandidates[static_cast<size_t>(j)]);
    }

    int riversCreated = 0;
    std::vector<bool> riverVisited(static_cast<size_t>(totalTiles), false);

    for (int startIdx : highCandidates) {
        if (riversCreated >= config.riverCount) {
            break;
        }
        if (riverVisited[static_cast<size_t>(startIdx)]) {
            continue;
        }

        int curX = startIdx % width;
        int curY = startIdx / width;

        std::vector<int> path;
        path.push_back(startIdx);
        riverVisited[static_cast<size_t>(startIdx)] = true;

        bool reachedWater = false;
        for (int step = 0; step < config.maxRiverLength; ++step) {
            static const int DXY[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

            // Kiểm tra xem có ô Biển hoặc Hồ kề sát không -> nếu có thì sông kết thúc tại đây
            bool hitWaterNeighbor = false;
            for (int d = 0; d < 4; ++d) {
                int nx = curX + DXY[d][0];
                int ny = curY + DXY[d][1];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                int nIdx = ny * width + nx;
                sim::TerrainType nt = map.getTileByIndex(nIdx);
                if (nt == sim::TerrainType::Sea || nt == sim::TerrainType::Lake) {
                    hitWaterNeighbor = true;
                    break;
                }
            }
            if (hitWaterNeighbor) {
                reachedWater = true;
                break;
            }

            // Tìm hướng láng giềng chưa thăm có độ cao dốc nhất đi xuống
            int bestNeighbor = -1;
            int32_t lowestElev = heights[static_cast<size_t>(curY * width + curX)];

            for (int d = 0; d < 4; ++d) {
                int nx = curX + DXY[d][0];
                int ny = curY + DXY[d][1];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                int nIdx = ny * width + nx;
                if (riverVisited[static_cast<size_t>(nIdx)]) continue;

                if (heights[static_cast<size_t>(nIdx)] < lowestElev) {
                    lowestElev = heights[static_cast<size_t>(nIdx)];
                    bestNeighbor = nIdx;
                }
            }

            if (bestNeighbor == -1) {
                // Đạt điểm cực tiểu cục bộ (vùng trũng không lối thoát) -> tạo hồ nhỏ tại đây
                map.setTileByIndex(curY * width + curX, sim::TerrainType::Lake);
                reachedWater = true;
                break;
            }

            curX = bestNeighbor % width;
            curY = bestNeighbor / width;
            path.push_back(bestNeighbor);
            riverVisited[static_cast<size_t>(bestNeighbor)] = true;

            if (map.getTileByIndex(bestNeighbor) == sim::TerrainType::River) {
                // Nhập dòng vào một con sông khác
                reachedWater = true;
                break;
            }
        }

        // Nếu chiều dài sông hợp lệ, ghi nhận các ô trên đường đi là River
        if (static_cast<int>(path.size()) >= config.minRiverLength) {
            for (int pIdx : path) {
                sim::TerrainType currentT = map.getTileByIndex(pIdx);
                // Tuyệt đối không ghi đè lên Biển hay Hồ
                if (currentT != sim::TerrainType::Sea && currentT != sim::TerrainType::Lake) {
                    map.setTileByIndex(pIdx, sim::TerrainType::River);
                }
            }
            riversCreated++;
        }
    }
}

} // namespace mapgen
