#include "MapGenerator.hpp"
#include "FastRng.hpp"
#include "SimplexNoise.hpp"
#include "sim/map/WorldMap.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>
#include <vector>

namespace mapgen {

namespace {

struct Point {
    float x{0.0f};
    float y{0.0f};
};

inline float crossProduct(float x1, float y1, float x2, float y2, float px, float py) {
    return (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
}

inline float distToSegmentSq(float px, float py, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float l2 = dx * dx + dy * dy;
    if (l2 < 1e-6f) {
        float ex = px - x1;
        float ey = py - y1;
        return ex * ex + ey * ey;
    }
    float t = ((px - x1) * dx + (py - y1) * dy) / l2;
    t = std::clamp(t, 0.0f, 1.0f);
    float qx = x1 + t * dx;
    float qy = y1 + t * dy;
    float ex = px - qx;
    float ey = py - qy;
    return ex * ex + ey * ey;
}

Point pickEdgePoint(int edge, int width, int height, FastRng& rng) {
    // 0: Top, 1: Bottom, 2: Left, 3: Right
    switch (edge) {
        case 0: // Top
            return {static_cast<float>(rng.range(0, width - 1)), 0.0f};
        case 1: // Bottom
            return {static_cast<float>(rng.range(0, width - 1)), static_cast<float>(height - 1)};
        case 2: // Left
            return {0.0f, static_cast<float>(rng.range(0, height - 1))};
        case 3: // Right
        default:
            return {static_cast<float>(width - 1), static_cast<float>(rng.range(0, height - 1))};
    }
}

// Hàm flood-fill tìm tất cả các ô đất liên thông theo 8 hướng
std::vector<bool> floodFill8Land(const sim::WorldMap& map, int startX, int startY, int& outCount) {
    int w = map.getWidth();
    int h = map.getHeight();
    std::vector<bool> visited(static_cast<size_t>(w * h), false);
    outCount = 0;

    auto isLand = [&](int x, int y) -> bool {
        sim::TerrainType t = map.getTile(x, y);
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

// Helper CRC32 và Adler32 cho chuẩn nén PNG độc lập
uint32_t crc32Update(uint32_t crc, const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc;
}

uint32_t computeCrc32(const uint8_t* buf, size_t len) {
    return ~crc32Update(0xFFFFFFFF, buf, len);
}

uint32_t computeAdler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

void writeBe32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void writePngChunk(std::ofstream& out, const char type[4], const uint8_t* data, size_t len) {
    std::vector<uint8_t> header;
    writeBe32(header, static_cast<uint32_t>(len));
    out.write(reinterpret_cast<const char*>(header.data()), 4);
    out.write(type, 4);

    if (len > 0 && data) {
        out.write(reinterpret_cast<const char*>(data), len);
    }

    uint32_t crc = crc32Update(0xFFFFFFFF, reinterpret_cast<const uint8_t*>(type), 4);
    if (len > 0 && data) {
        crc = crc32Update(crc, data, len);
    }
    crc = ~crc;

    std::vector<uint8_t> crcBuf;
    writeBe32(crcBuf, crc);
    out.write(reinterpret_cast<const char*>(crcBuf.data()), 4);
}

} // namespace

void MapGenerator::generate(sim::WorldMap& map, uint64_t seed, const MapGenConfig& config) const {
    const int width = config.width;
    const int height = config.height;
    const int totalTiles = width * height;

    map.resize(width, height, map.getTileSize());

    std::vector<int32_t> heights(static_cast<size_t>(totalTiles), 0);

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
    // 5. SÔNG (River) - Luôn chia bản đồ làm 2 phần
    // =========================================================================
    Point p1{0.0f, 0.0f};
    Point p2{0.0f, 0.0f};
    std::vector<bool> isRiver(static_cast<size_t>(totalTiles), false);
    std::vector<Point> riverCenterline;

    bool riverFound = false;
    FastRng riverRng(seed ^ 0xA5A55A5AFEEDULL);

    const Point basePlayer{static_cast<float>(config.playerBaseX), static_cast<float>(config.playerBaseY)};
    const Point baseEnemy{static_cast<float>(config.enemyBaseX), static_cast<float>(config.enemyBaseY)};

    for (int attempt = 0; attempt < config.riverMaxRetries; ++attempt) {
        if (config.fixedP1X >= 0 && config.fixedP1Y >= 0 && config.fixedP2X >= 0 && config.fixedP2Y >= 0) {
            p1 = {static_cast<float>(config.fixedP1X), static_cast<float>(config.fixedP1Y)};
            p2 = {static_cast<float>(config.fixedP2X), static_cast<float>(config.fixedP2Y)};
        } else {
            // Chọn ngẫu nhiên 2 điểm trên BIÊN của map nằm ở 2 cạnh khác nhau
            int e1 = riverRng.range(0, 3);
            int e2 = (e1 + riverRng.range(1, 3)) % 4;
            p1 = pickEdgePoint(e1, width, height, riverRng);
            p2 = pickEdgePoint(e2, width, height, riverRng);
        }

        // Kiểm tra căn cứ 2 phe phải nằm ở 2 nửa khác nhau bằng dấu của tích có hướng
        float cpPlayer = crossProduct(p1.x, p1.y, p2.x, p2.y, basePlayer.x, basePlayer.y);
        float cpEnemy  = crossProduct(p1.x, p1.y, p2.x, p2.y, baseEnemy.x, baseEnemy.y);

        if ((cpPlayer > 0.0f && cpEnemy > 0.0f) || (cpPlayer < 0.0f && cpEnemy < 0.0f) ||
            std::abs(cpPlayer) < 1.0f || std::abs(cpEnemy) < 1.0f) {
            // Không nằm ở 2 nửa khác nhau hoặc trục sông đi xuyên qua căn cứ -> sinh lại
            if (config.fixedP1X < 0) {
                continue;
            }
        }

        // Đảm bảo trục sông không đi sát hoặc xuyên qua vùng căn cứ của 2 phe
        float distSqP = distToSegmentSq(basePlayer.x, basePlayer.y, p1.x, p1.y, p2.x, p2.y);
        float distSqE = distToSegmentSq(baseEnemy.x, baseEnemy.y, p1.x, p1.y, p2.x, p2.y);
        float minBaseClearance = static_cast<float>(config.baseRadius) + config.riverMaxDeviation + 2.0f;
        if (config.fixedP1X < 0) {
            if (distSqP < minBaseClearance * minBaseClearance || distSqE < minBaseClearance * minBaseClearance) {
                continue;
            }
        }

        // Tạo đường sông uốn lượn nhẹ bằng Simplex noise
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::hypot(dx, dy);
        if (len < 2.0f) continue;

        float ux = dx / len;
        float uy = dy / len;
        float nx = -uy;
        float ny = ux;

        int numSteps = std::max(static_cast<int>(len * 2.5f), 10);
        float ds = len / static_cast<float>(numSteps);

        SimplexNoise meanderNoise(seed + 99911ULL + static_cast<uint64_t>(attempt * 77ULL));
        std::vector<Point> candidateCenterline;
        candidateCenterline.reserve(static_cast<size_t>(numSteps + 1));

        for (int s = 0; s <= numSteps; ++s) {
            float distAlong = static_cast<float>(s) * ds;
            float t = distAlong / len; // 0..1
            // Envelope hình sin đảm bảo độ lệch tại 2 đầu mút luôn = 0 (khớp chính xác mép biên)
            float envelope = std::sin(3.14159265f * t);

            int32_t noiseVal = meanderNoise.sampleOctaves(
                static_cast<int>(distAlong * 3.0f),
                static_cast<int>(attempt * 20),
                2,
                1200,
                16384
            );
            float nNorm = static_cast<float>(noiseVal) / 32768.0f; // -1..1
            float dev = config.riverMaxDeviation * envelope * nNorm;

            float cx = p1.x + distAlong * ux + dev * nx;
            float cy = p1.y + distAlong * uy + dev * ny;
            candidateCenterline.push_back({cx, cy});
        }

        // Rasterize sông rộng 2-3 ô (bán kính R ~ 1.25 ô tạo độ rộng đường quét 2.5 ô)
        std::vector<bool> candidateRiverMask(static_cast<size_t>(totalTiles), false);
        constexpr float RIVER_RADIUS_SQ = 1.25f * 1.25f;

        for (size_t seg = 0; seg + 1 < candidateCenterline.size(); ++seg) {
            const auto& a = candidateCenterline[seg];
            const auto& b = candidateCenterline[seg + 1];

            int minX = std::max(0, static_cast<int>(std::floor(std::min(a.x, b.x) - 2.5f)));
            int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max(a.x, b.x) + 2.5f)));
            int minY = std::max(0, static_cast<int>(std::floor(std::min(a.y, b.y) - 2.5f)));
            int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max(a.y, b.y) + 2.5f)));

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    float px = static_cast<float>(x) + 0.5f;
                    float py = static_cast<float>(y) + 0.5f;

                    // Không cho sông đè vào vùng căn cứ xuất phát
                    float dP = std::hypot(px - basePlayer.x, py - basePlayer.y);
                    float dE = std::hypot(px - baseEnemy.x, py - baseEnemy.y);
                    if (dP <= static_cast<float>(config.baseRadius) || dE <= static_cast<float>(config.baseRadius)) {
                        continue;
                    }

                    if (distToSegmentSq(px, py, a.x, a.y, b.x, b.y) <= RIVER_RADIUS_SQ) {
                        candidateRiverMask[static_cast<size_t>(y * width + x)] = true;
                    }
                }
            }
        }

        // Bịt kín hoàn toàn các ô tiếp xúc chéo (chống lọt quân khi đi 8 hướng)
        for (int y = 0; y < height - 1; ++y) {
            for (int x = 0; x < width - 1; ++x) {
                bool r00 = candidateRiverMask[static_cast<size_t>(y * width + x)];
                bool r10 = candidateRiverMask[static_cast<size_t>(y * width + (x + 1))];
                bool r01 = candidateRiverMask[static_cast<size_t>((y + 1) * width + x)];
                bool r11 = candidateRiverMask[static_cast<size_t>((y + 1) * width + (x + 1))];

                if (r00 && r11 && !r10 && !r01) {
                    candidateRiverMask[static_cast<size_t>(y * width + (x + 1))] = true;
                    candidateRiverMask[static_cast<size_t>((y + 1) * width + x)] = true;
                } else if (!r00 && !r11 && r10 && r01) {
                    candidateRiverMask[static_cast<size_t>(y * width + x)] = true;
                    candidateRiverMask[static_cast<size_t>((y + 1) * width + (x + 1))] = true;
                }
            }
        }

        // Kiểm tra điều kiện diện tích đất mỗi nửa >= 40% bằng flood-fill thực tế từ 2 căn cứ
        sim::WorldMap tempMap = map;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = static_cast<size_t>(y * width + x);
                if (candidateRiverMask[idx] && tempMap.getTileByIndex(static_cast<int>(idx)) != sim::TerrainType::Sea) {
                    tempMap.setTileByIndex(static_cast<int>(idx), sim::TerrainType::River);
                }
            }
        }

        int countReachP = 0;
        int countReachE = 0;
        floodFill8Land(tempMap, config.playerBaseX, config.playerBaseY, countReachP);
        floodFill8Land(tempMap, config.enemyBaseX, config.enemyBaseY, countReachE);

        const int minRequiredLand = static_cast<int>(std::floor(static_cast<float>(landCount) * config.minLandRatio)); // 40% = 4096 ô
        if (config.fixedP1X < 0) {
            if (countReachP < minRequiredLand || countReachE < minRequiredLand) {
                // Vi phạm diện tích đất tối thiểu 40% -> sinh lại trục sông
                continue;
            }
        }

        isRiver = std::move(candidateRiverMask);
        riverCenterline = std::move(candidateCenterline);
        riverFound = true;
        break;
    }

    // Fallback an toàn tuyệt đối nếu vượt quá số lần thử: Trục sông thẳng cắt đôi theo trục giữa
    if (!riverFound) {
        p1 = {static_cast<float>(width / 2), 0.0f};
        p2 = {static_cast<float>(width / 2), static_cast<float>(height - 1)};
        isRiver.assign(static_cast<size_t>(totalTiles), false);
        riverCenterline.clear();
        int midX = width / 2;
        for (int y = 0; y < height; ++y) {
            isRiver[static_cast<size_t>(y * width + (midX - 1))] = true;
            isRiver[static_cast<size_t>(y * width + midX)] = true;
            isRiver[static_cast<size_t>(y * width + (midX + 1))] = true;
            riverCenterline.push_back({static_cast<float>(midX), static_cast<float>(y)});
        }
    }

    // Gán các ô đất thuộc phạm vi sông thành River (không ghi đè lên Biển để bảo tồn đúng tỉ lệ biển)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y * width + x);
            if (isRiver[idx] && map.getTileByIndex(static_cast<int>(idx)) != sim::TerrainType::Sea) {
                map.setTileByIndex(static_cast<int>(idx), sim::TerrainType::River);
            }
        }
    }

    // Tạo N chỗ cạn (ford) vượt sông nếu config yêu cầu
    if (config.riverFordCount > 0 && !riverCenterline.empty()) {
        std::vector<Point> eligibleFordCandidates;
        const float minEndDist = static_cast<float>(config.riverFordMinDistance);

        for (const auto& pt : riverCenterline) {
            float d1 = std::hypot(pt.x - p1.x, pt.y - p1.y);
            float d2 = std::hypot(pt.x - p2.x, pt.y - p2.y);
            if (d1 >= minEndDist && d2 >= minEndDist) {
                int tx = std::clamp(static_cast<int>(pt.x), 0, width - 1);
                int ty = std::clamp(static_cast<int>(pt.y), 0, height - 1);
                // Chỉ đặt bến cạn ở nơi sông chảy qua đất liền
                if (map.getTile(tx, ty) == sim::TerrainType::River) {
                    eligibleFordCandidates.push_back(pt);
                }
            }
        }

        std::vector<Point> chosenFords;
        if (!eligibleFordCandidates.empty()) {
            // Xáo trộn tất định các ứng viên bến cạn
            for (int i = static_cast<int>(eligibleFordCandidates.size()) - 1; i > 0; --i) {
                int j = riverRng.range(0, i);
                std::swap(eligibleFordCandidates[static_cast<size_t>(i)], eligibleFordCandidates[static_cast<size_t>(j)]);
            }

            for (const auto& cand : eligibleFordCandidates) {
                if (static_cast<int>(chosenFords.size()) >= config.riverFordCount) break;

                bool tooClose = false;
                for (const auto& existing : chosenFords) {
                    if (std::hypot(cand.x - existing.x, cand.y - existing.y) < static_cast<float>(config.riverFordMinDistance)) {
                        tooClose = true;
                        break;
                    }
                }
                if (!tooClose) {
                    chosenFords.push_back(cand);
                }
            }

            // Gán các ô sông tại vị trí bến cạn thành Ford
            for (const auto& fordPt : chosenFords) {
                int fx = static_cast<int>(std::round(fordPt.x));
                int fy = static_cast<int>(std::round(fordPt.y));
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = fx + dx;
                        int ny = fy + dy;
                        if (map.isInBounds(nx, ny) && map.getTile(nx, ny) == sim::TerrainType::River) {
                            map.setTile(nx, ny, sim::TerrainType::Ford);
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // 6. HỒ (Lake) - Ngẫu nhiên từ 0 đến lakeMax, không đè lên sông, căn cứ, biển,
    //    và không làm đứt liên thông đất trong cùng một nửa
    // =========================================================================
    int targetLakeCount = (config.lakeMax > 0) ? riverRng.range(0, config.lakeMax) : 0;
    int lakesPlaced = 0;

    // Xác định dấu của nửa cho căn cứ Player và Enemy
    float cpPlayer = crossProduct(p1.x, p1.y, p2.x, p2.y, basePlayer.x, basePlayer.y);
    bool playerSidePositive = (cpPlayer > 0.0f);

    for (int attempt = 0; attempt < 200 && lakesPlaced < targetLakeCount; ++attempt) {
        int cx = riverRng.range(12, width - 13);
        int cy = riverRng.range(12, height - 13);

        sim::TerrainType centerT = map.getTile(cx, cy);
        if (centerT != sim::TerrainType::Plain && centerT != sim::TerrainType::Hill && centerT != sim::TerrainType::Forest) {
            continue;
        }

        // Không đè hoặc nằm sát vùng xuất phát của 2 phe
        float distToBase1 = std::hypot(static_cast<float>(cx) - basePlayer.x, static_cast<float>(cy) - basePlayer.y);
        float distToBase2 = std::hypot(static_cast<float>(cx) - baseEnemy.x, static_cast<float>(cy) - baseEnemy.y);
        if (distToBase1 < static_cast<float>(config.baseRadius + 3) ||
            distToBase2 < static_cast<float>(config.baseRadius + 3)) {
            continue;
        }

        int lakeRadius = riverRng.range(config.lakeMinRadius, config.lakeMaxRadius);

        // Thu thập các ô dự kiến tạo hồ
        std::vector<std::pair<int, int>> candidateLakeTiles;
        bool touchesProhibited = false;

        for (int dy = -lakeRadius; dy <= lakeRadius; ++dy) {
            for (int dx = -lakeRadius; dx <= lakeRadius; ++dx) {
                if (dx * dx + dy * dy <= lakeRadius * lakeRadius) {
                    int tx = cx + dx;
                    int ty = cy + dy;
                    if (!map.isInBounds(tx, ty)) {
                        touchesProhibited = true;
                        break;
                    }

                    sim::TerrainType t = map.getTile(tx, ty);
                    // Hồ không được đè lên biển, sông, bến cạn
                    if (t == sim::TerrainType::Sea || t == sim::TerrainType::River ||
                        t == sim::TerrainType::Ford || t == sim::TerrainType::Lake) {
                        touchesProhibited = true;
                        break;
                    }

                    // Hồ không được tiếp giáp trực tiếp với Biển (hồ không nối biển) hoặc Sông
                    for (int ndy = -1; ndy <= 1; ++ndy) {
                        for (int ndx = -1; ndx <= 1; ++ndx) {
                            int nx = tx + ndx;
                            int ny = ty + ndy;
                            if (map.isInBounds(nx, ny)) {
                                sim::TerrainType nt = map.getTile(nx, ny);
                                if (nt == sim::TerrainType::Sea || nt == sim::TerrainType::River || nt == sim::TerrainType::Ford) {
                                    touchesProhibited = true;
                                    break;
                                }
                            }
                        }
                        if (touchesProhibited) break;
                    }
                    if (touchesProhibited) break;

                    candidateLakeTiles.emplace_back(tx, ty);
                }
            }
            if (touchesProhibited) break;
        }

        if (touchesProhibited || candidateLakeTiles.empty()) {
            continue;
        }

        // Xác định nửa bản đồ chứa hồ này
        float cpHole = crossProduct(p1.x, p1.y, p2.x, p2.y, static_cast<float>(cx), static_cast<float>(cy));
        bool lakeSidePositive = (cpHole > 0.0f);
        bool sameSideAsPlayer = (lakeSidePositive == playerSidePositive);
        int anchorX = sameSideAsPlayer ? config.playerBaseX : config.enemyBaseX;
        int anchorY = sameSideAsPlayer ? config.playerBaseY : config.enemyBaseY;

        // Đếm số ô đất liên thông TRƯỚC khi đặt hồ
        int reachableBefore = 0;
        floodFill8Land(map, anchorX, anchorY, reachableBefore);

        // Tạm thời đặt các ô của hồ thành Lake
        std::vector<sim::TerrainType> backupTypes;
        backupTypes.reserve(candidateLakeTiles.size());
        for (const auto& [tx, ty] : candidateLakeTiles) {
            backupTypes.push_back(map.getTile(tx, ty));
            map.setTile(tx, ty, sim::TerrainType::Lake);
        }

        // Đếm số ô đất liên thông SAU khi đặt hồ
        int reachableAfter = 0;
        floodFill8Land(map, anchorX, anchorY, reachableAfter);

        // Kiểm tra xem hồ có làm đứt liên thông của đất trong cùng một nửa không
        // Nếu số ô liên thông bị giảm nhiều hơn số ô hồ được chuyển đổi -> đã làm đứt liên thông
        if (reachableAfter == reachableBefore - static_cast<int>(candidateLakeTiles.size())) {
            // Giữ nguyên hồ thành công
            lakesPlaced++;
        } else {
            // Đã làm đứt liên thông -> phục hồi lại địa hình cũ và bỏ hồ đó
            for (size_t k = 0; k < candidateLakeTiles.size(); ++k) {
                map.setTile(candidateLakeTiles[k].first, candidateLakeTiles[k].second, backupTypes[k]);
            }
        }
    }
}

bool MapGenerator::exportPng(const sim::WorldMap& map, const std::string& filepath, int scale) {
    if (scale < 1) scale = 1;
    int mapW = map.getWidth();
    int mapH = map.getHeight();
    int imgW = mapW * scale;
    int imgH = mapH * scale;

    auto getColorRgb = [](sim::TerrainType t) -> std::tuple<uint8_t, uint8_t, uint8_t> {
        switch (t) {
            case sim::TerrainType::Sea:      return {30, 75, 155};
            case sim::TerrainType::Plain:    return {115, 175, 75};
            case sim::TerrainType::Forest:   return {45, 110, 50};
            case sim::TerrainType::Hill:     return {180, 160, 95};
            case sim::TerrainType::Mountain: return {140, 130, 125};
            case sim::TerrainType::River:    return {65, 135, 215};
            case sim::TerrainType::Lake:     return {50, 115, 195};
            case sim::TerrainType::Ford:     return {130, 195, 220};
            default:                         return {80, 80, 80};
        }
    };

    // Chuẩn bị dữ liệu thô: mỗi hàng có 1 byte filter type (0x00) + imgW * 3 bytes RGB
    std::vector<uint8_t> rawData;
    rawData.reserve(static_cast<size_t>(imgH * (1 + imgW * 3)));

    for (int y = 0; y < imgH; ++y) {
        rawData.push_back(0); // Filter type None
        int ty = y / scale;
        for (int x = 0; x < imgW; ++x) {
            int tx = x / scale;
            auto [r, g, b] = getColorRgb(map.getTile(tx, ty));
            rawData.push_back(r);
            rawData.push_back(g);
            rawData.push_back(b);
        }
    }

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // 1. PNG Signature
    const uint8_t signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    out.write(reinterpret_cast<const char*>(signature), 8);

    // 2. IHDR Chunk
    std::vector<uint8_t> ihdr;
    writeBe32(ihdr, static_cast<uint32_t>(imgW));
    writeBe32(ihdr, static_cast<uint32_t>(imgH));
    ihdr.push_back(8); // Bit depth
    ihdr.push_back(2); // Color type RGB
    ihdr.push_back(0); // Compression
    ihdr.push_back(0); // Filter
    ihdr.push_back(0); // Interlace
    writePngChunk(out, "IHDR", ihdr.data(), ihdr.size());

    // 3. IDAT Chunk (zlib Deflate uncompressed blocks)
    std::vector<uint8_t> idat;
    idat.push_back(0x78); // zlib CMF (Deflate, 32K window)
    idat.push_back(0x01); // zlib FLG

    size_t offset = 0;
    while (offset < rawData.size()) {
        size_t chunkSize = std::min(rawData.size() - offset, static_cast<size_t>(65535));
        bool isLast = (offset + chunkSize >= rawData.size());

        idat.push_back(isLast ? 0x01 : 0x00); // BFINAL & BTYPE (00 = no compression)
        uint16_t lenU16 = static_cast<uint16_t>(chunkSize);
        uint16_t nlenU16 = ~lenU16;
        idat.push_back(static_cast<uint8_t>(lenU16 & 0xFF));
        idat.push_back(static_cast<uint8_t>((lenU16 >> 8) & 0xFF));
        idat.push_back(static_cast<uint8_t>(nlenU16 & 0xFF));
        idat.push_back(static_cast<uint8_t>((nlenU16 >> 8) & 0xFF));

        idat.insert(idat.end(), rawData.begin() + static_cast<ptrdiff_t>(offset),
                                rawData.begin() + static_cast<ptrdiff_t>(offset + chunkSize));
        offset += chunkSize;
    }

    uint32_t adler = computeAdler32(rawData.data(), rawData.size());
    writeBe32(idat, adler);
    writePngChunk(out, "IDAT", idat.data(), idat.size());

    // 4. IEND Chunk
    writePngChunk(out, "IEND", nullptr, 0);

    return true;
}

std::string MapGenerator::exportAscii(const sim::WorldMap& map, int p1X, int p1Y, int p2X, int p2Y) {
    int w = map.getWidth();
    int h = map.getHeight();
    std::ostringstream oss;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (x == p1X && y == p1Y) {
                oss << 'P';
                continue;
            }
            if (x == p2X && y == p2Y) {
                oss << 'E';
                continue;
            }

            sim::TerrainType t = map.getTile(x, y);
            switch (t) {
                case sim::TerrainType::Sea:      oss << '~'; break;
                case sim::TerrainType::Plain:    oss << '.'; break;
                case sim::TerrainType::Forest:   oss << 'T'; break;
                case sim::TerrainType::Hill:     oss << '^'; break;
                case sim::TerrainType::Mountain: oss << 'A'; break;
                case sim::TerrainType::River:    oss << '='; break;
                case sim::TerrainType::Lake:     oss << 'O'; break;
                case sim::TerrainType::Ford:     oss << '#'; break;
                default:                         oss << '?'; break;
            }
        }
        oss << '\n';
    }
    return oss.str();
}

} // namespace mapgen
