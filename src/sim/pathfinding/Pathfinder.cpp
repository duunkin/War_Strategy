#include "sim/pathfinding/Pathfinder.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

namespace sim {

struct AStarNode {
    TileCoord coord;
    int fCost{0};
    int gCost{0};

    bool operator>(const AStarNode& o) const {
        return fCost > o.fCost;
    }
};

namespace {

struct PathScratch {
    std::vector<uint32_t> visitedStamp;
    std::vector<int>      gCosts;
    std::vector<int>      parent;
    std::vector<uint32_t> closedStamp;
    std::vector<AStarNode> openHeap;
    uint32_t              currentStamp{1};

    void ensureCapacity(size_t totalTiles) {
        if (visitedStamp.size() < totalTiles) {
            visitedStamp.assign(totalTiles, 0);
            gCosts.resize(totalTiles, 0);
            parent.resize(totalTiles, -1);
            closedStamp.assign(totalTiles, 0);
            currentStamp = 1;
        }
    }

    void reset(size_t totalTiles) {
        ensureCapacity(totalTiles);
        currentStamp++;
        if (currentStamp == 0) { // Xử lý tràn số hiếm gặp sau 4 tỷ lần gọi
            visitedStamp.assign(totalTiles, 0);
            closedStamp.assign(totalTiles, 0);
            currentStamp = 1;
        }
        openHeap.clear();
    }
};

static thread_local PathScratch tl_scratch;

inline DetailedTerrainType getFastDetailedTerrain(const WorldMap& map, const SimConfig& config,
                                                 int x, int y, UnitDomain domain) {
    TerrainType raw = map.getTile(x, y);
    if (domain == UnitDomain::Land) {
        if (raw == TerrainType::Sea) return DetailedTerrainType::DeepSea; // Biển không thể đi với lục quân
        if (raw == TerrainType::River || raw == TerrainType::Lake) return DetailedTerrainType::River; // Sông/Hồ không đi được với lục quân
        if (raw == TerrainType::Ford) return DetailedTerrainType::Ford; // Chỗ cạn vượt sông
        return config.classifyTerrain(map, x, y);
    }
    if (domain == UnitDomain::Sea) {
        if (raw != TerrainType::Sea) return DetailedTerrainType::Plain; // Tàu không vào hồ, sông, đất liền
        return config.classifyTerrain(map, x, y);
    }
    if (raw == TerrainType::Ford) return DetailedTerrainType::Ford;
    return config.classifyTerrain(map, x, y);
}

} // namespace

PathResult Pathfinder::findPath(const WorldMap& map,
                               const SimConfig& config,
                               UnitDomain domain,
                               UnitType unitType,
                               TileCoord start,
                               TileCoord target) {
    PathResult result;
    result.originalTarget = target;
    result.finalTarget = target;

    int width = map.getWidth();
    int height = map.getHeight();

    if (!map.isInBounds(start.x, start.y) || !map.isInBounds(target.x, target.y)) {
        return result;
    }

    if (start == target) {
        result.found = true;
        result.finalTarget = target;
        return result;
    }

    // 1. Máy bay: bay thẳng, bỏ qua địa hình
    if (domain == UnitDomain::Air) {
        result.found = true;
        result.adjusted = false;
        result.finalTarget = target;
        result.waypoints = {target};
        return result;
    }

    // 2. Tìm đường A* cho bộ binh, xe cơ giới, pháo, tư lệnh và tàu
    size_t totalTiles = static_cast<size_t>(width * height);
    constexpr int INF_COST = 1000000000;

    tl_scratch.reset(totalTiles);
    auto& openHeap = tl_scratch.openHeap;
    uint32_t stamp = tl_scratch.currentStamp;

    auto getIdx = [width](int x, int y) -> size_t {
        return static_cast<size_t>(y * width + x);
    };

    auto getCoord = [width](size_t idx) -> TileCoord {
        return TileCoord{static_cast<int>(idx % width), static_cast<int>(idx / width)};
    };

    auto octileHeuristic = [](int x1, int y1, int x2, int y2) -> int {
        int dx = std::abs(x1 - x2);
        int dy = std::abs(y1 - y2);
        return std::min(dx, dy) * 1414 + std::abs(dx - dy) * 1000;
    };

    size_t startIdx = getIdx(start.x, start.y);
    tl_scratch.visitedStamp[startIdx] = stamp;
    tl_scratch.gCosts[startIdx] = 0;
    openHeap.push_back({start, octileHeuristic(start.x, start.y, target.x, target.y), 0});

    // Theo dõi ô đến được có khoảng cách Euclid gần đích target nhất để phục vụ fallback khi đích không đến được
    TileCoord closestReachable = start;
    int64_t minDistanceToTargetSq = static_cast<int64_t>(start.x - target.x) * (start.x - target.x) +
                                    static_cast<int64_t>(start.y - target.y) * (start.y - target.y);

    bool reachedTarget = false;

    // 8 hướng di chuyển: 4 hướng thẳng và 4 hướng chéo
    constexpr int dx[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    constexpr int dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};

    while (!openHeap.empty()) {
        std::pop_heap(openHeap.begin(), openHeap.end(), std::greater<AStarNode>());
        AStarNode current = openHeap.back();
        openHeap.pop_back();

        size_t currentIdx = getIdx(current.coord.x, current.coord.y);
        if (tl_scratch.closedStamp[currentIdx] == stamp) continue;
        tl_scratch.closedStamp[currentIdx] = stamp;

        // Cập nhật ô đến được gần đích nhất
        int64_t dSq = static_cast<int64_t>(current.coord.x - target.x) * (current.coord.x - target.x) +
                      static_cast<int64_t>(current.coord.y - target.y) * (current.coord.y - target.y);
        if (dSq < minDistanceToTargetSq) {
            minDistanceToTargetSq = dSq;
            closestReachable = current.coord;
        }

        if (current.coord == target) {
            reachedTarget = true;
            break;
        }

        for (int dir = 0; dir < 8; ++dir) {
            int nx = current.coord.x + dx[dir];
            int ny = current.coord.y + dy[dir];

            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

            size_t nextIdx = getIdx(nx, ny);
            if (tl_scratch.closedStamp[nextIdx] == stamp) continue;

            DetailedTerrainType dt = getFastDetailedTerrain(map, config, nx, ny, domain);
            int speedPercent = config.getSpeedMultiplierPercent(dt, domain, unitType);
            if (speedPercent <= 0) continue; // Ô không đi được

            bool isDiagonal = (dir >= 4);

            // Kiểm tra luật cắt góc: không được cắt góc qua hai ô chéo bị chặn
            if (isDiagonal) {
                int adjX1 = current.coord.x + dx[dir];
                int adjY1 = current.coord.y;
                int adjX2 = current.coord.x;
                int adjY2 = current.coord.y + dy[dir];

                DetailedTerrainType dt1 = getFastDetailedTerrain(map, config, adjX1, adjY1, domain);
                DetailedTerrainType dt2 = getFastDetailedTerrain(map, config, adjX2, adjY2, domain);

                bool pass1 = config.isPassable(dt1, domain, unitType);
                bool pass2 = config.isPassable(dt2, domain, unitType);

                // Cả hai ô kề đều bị chặn -> cấm đi chéo cắt góc
                if (!pass1 && !pass2) {
                    continue;
                }
            }

            // Chi phí ô = (100 * 1000) / speedPercent
            // Đi chéo chi phí x1.414 (nhân 1414 / 1000)
            int baseStepCost = (100 * 1000) / speedPercent;
            int stepCost = isDiagonal ? ((baseStepCost * 1414) / 1000) : baseStepCost;

            int tentativeG = current.gCost + stepCost;
            int currentG = (tl_scratch.visitedStamp[nextIdx] == stamp) ? tl_scratch.gCosts[nextIdx] : INF_COST;

            if (tentativeG < currentG) {
                tl_scratch.visitedStamp[nextIdx] = stamp;
                tl_scratch.gCosts[nextIdx] = tentativeG;
                tl_scratch.parent[nextIdx] = static_cast<int>(currentIdx);
                int h = octileHeuristic(nx, ny, target.x, target.y);
                openHeap.push_back({TileCoord{nx, ny}, tentativeG + h, tentativeG});
                std::push_heap(openHeap.begin(), openHeap.end(), std::greater<AStarNode>());
            }
        }
    }

    TileCoord endCoord = reachedTarget ? target : closestReachable;
    result.found = (reachedTarget || (endCoord != start));
    result.adjusted = !reachedTarget;
    result.finalTarget = endCoord;

    if (!result.found) {
        return result;
    }

    // Tái cấu trúc đường đi từ endCoord ngược về start
    std::vector<TileCoord> path;
    TileCoord curr = endCoord;
    while (curr != start) {
        path.push_back(curr);
        size_t cIdx = getIdx(curr.x, curr.y);
        int pIdx = (tl_scratch.visitedStamp[cIdx] == stamp) ? tl_scratch.parent[cIdx] : -1;
        if (pIdx < 0) break;
        curr = getCoord(static_cast<size_t>(pIdx));
    }

    std::reverse(path.begin(), path.end());
    result.waypoints = std::move(path);
    return result;
}

std::vector<TileCoord> Pathfinder::disperseDestinations(const WorldMap& map,
                                                       const SimConfig& config,
                                                       UnitDomain domain,
                                                       UnitType unitType,
                                                       TileCoord centerTarget,
                                                       size_t unitCount) {
    std::vector<TileCoord> destinations;
    if (unitCount == 0) return destinations;
    destinations.reserve(unitCount);

    int width = map.getWidth();
    int height = map.getHeight();

    // Nếu ô tâm centerTarget đi được, chọn nó đầu tiên
    DetailedTerrainType centerDt = getFastDetailedTerrain(map, config, centerTarget.x, centerTarget.y, domain);
    if (config.isPassable(centerDt, domain, unitType)) {
        destinations.push_back(centerTarget);
    }

    if (destinations.size() >= unitCount) {
        return destinations;
    }

    // Tìm kiếm các ô lân cận theo hình xoắn ốc xung quanh tâm
    int maxRadius = 15;
    for (int r = 1; r <= maxRadius && destinations.size() < unitCount; ++r) {
        // Duyệt chu vi hình vuông bán kính r
        for (int dy = -r; dy <= r && destinations.size() < unitCount; ++dy) {
            for (int dx = -r; dx <= r && destinations.size() < unitCount; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                int nx = centerTarget.x + dx;
                int ny = centerTarget.y + dy;

                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

                DetailedTerrainType dt = getFastDetailedTerrain(map, config, nx, ny, domain);
                if (!config.isPassable(dt, domain, unitType)) continue;

                TileCoord cand{nx, ny};
                bool exists = false;
                for (const auto& d : destinations) {
                    if (d == cand) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    destinations.push_back(cand);
                }
            }
        }
    }

    // Nếu không tìm đủ ô phân tán (ví dụ ở địa hình hẹp), điền các ô còn lại bằng ô gần nhất
    while (destinations.size() < unitCount) {
        destinations.push_back(!destinations.empty() ? destinations.front() : centerTarget);
    }

    return destinations;
}

} // namespace sim
