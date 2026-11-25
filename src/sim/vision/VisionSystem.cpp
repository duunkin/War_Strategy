#include "VisionSystem.hpp"
#include <cmath>
#include <algorithm>

namespace sim {

bool VisionSystem::isLineOfSightClear(int x0, int y0, int h0,
                                     int x1, int y1, int h1,
                                     const uint8_t* elevationGrid,
                                     int mapWidth,
                                     int airElevation) {
    if (h0 >= airElevation || h1 >= airElevation) {
        return true;
    }

    if (x0 == x1 && y0 == y1) {
        return true;
    }

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;
    int steps = std::max(dx, dy);
    int k = 0;

    while (x != x1 || y != y1) {
        if (x != x0 || y != y0) {
            int hMid = elevationGrid ? elevationGrid[y * mapWidth + x] : 0;
            if (hMid > 0) {
                int64_t losNum = static_cast<int64_t>(steps - k) * h0 + static_cast<int64_t>(k) * h1;
                int64_t midNum = static_cast<int64_t>(hMid) * steps;

                if (midNum > losNum) {
                    return false;
                }
                if (midNum == losNum && h0 == h1) {
                    return false;
                }
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        k++;
    }

    return true;
}

bool VisionSystem::isLineOfSightClear(int x0, int y0, int h0,
                                     int x1, int y1, int h1,
                                     const std::function<int(int, int)>& getElevationFunc,
                                     int airElevation) {
    if (h0 >= airElevation || h1 >= airElevation) {
        return true;
    }

    if (x0 == x1 && y0 == y1) {
        return true;
    }

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;
    int steps = std::max(dx, dy);
    int k = 0;

    while (x != x1 || y != y1) {
        if (x != x0 || y != y0) {
            int hMid = getElevationFunc ? getElevationFunc(x, y) : 0;
            if (hMid > 0) {
                int64_t losNum = static_cast<int64_t>(steps - k) * h0 + static_cast<int64_t>(k) * h1;
                int64_t midNum = static_cast<int64_t>(hMid) * steps;

                if (midNum > losNum) {
                    return false;
                }
                if (midNum == losNum && h0 == h1) {
                    return false;
                }
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        k++;
    }

    return true;
}

void VisionSystem::updateFactionVision(Faction faction,
                                       const std::vector<Unit>& units,
                                       const WorldMap& map,
                                       const SimConfig& config,
                                       const uint8_t* elevationGrid,
                                       std::vector<uint8_t>& fogMap,
                                       std::vector<uint16_t>* outVisibleIndices) {
    int width = map.getWidth();
    int height = map.getHeight();
    size_t totalTiles = static_cast<size_t>(width * height);

    if (fogMap.size() != totalTiles) {
        fogMap.assign(totalTiles, static_cast<uint8_t>(FogState::Unexplored));
    }

    // Bước 1: Chuyển các ô đang Visible (2) về Explored (1)
    if (outVisibleIndices && !outVisibleIndices->empty()) {
        for (uint16_t idx : *outVisibleIndices) {
            if (idx < totalTiles && fogMap[idx] == static_cast<uint8_t>(FogState::Visible)) {
                fogMap[idx] = static_cast<uint8_t>(FogState::Explored);
            }
        }
        outVisibleIndices->clear();
    } else {
        for (size_t i = 0; i < totalTiles; ++i) {
            if (fogMap[i] == static_cast<uint8_t>(FogState::Visible)) {
                fogMap[i] = static_cast<uint8_t>(FogState::Explored);
            }
        }
        if (outVisibleIndices) {
            outVisibleIndices->clear();
        }
    }

    int airElevation = config.getVisionConfig().airElevation;

    // Bước 2: Duyệt từng đơn vị còn sống của phe để mở tầm nhìn Visible
    for (const auto& u : units) {
        if (u.isDead() || u.getFaction() != faction) {
            continue;
        }

        int ux = u.getTileX();
        int uy = u.getTileY();
        if (!map.isInBounds(ux, uy)) continue;

        DetailedTerrainType standDt = config.classifyTerrain(map, ux, uy);
        int visionMod = config.getTerrainProperty(standDt).visionMod;
        int rEff = std::max(2, u.getVisionTiles() + visionMod);
        int rEffSq = rEff * rEff;

        int h0 = (u.getDomain() == UnitDomain::Air) ? airElevation
                : (elevationGrid ? elevationGrid[uy * width + ux] : config.getTerrainProperty(standDt).elevation);

        int minX = std::max(0, ux - rEff);
        int maxX = std::min(width - 1, ux + rEff);
        int minY = std::max(0, uy - rEff);
        int maxY = std::min(height - 1, uy + rEff);

        for (int ty = minY; ty <= maxY; ++ty) {
            int dy = ty - uy;
            int dySq = dy * dy;
            for (int tx = minX; tx <= maxX; ++tx) {
                int dx = tx - ux;
                if (dx * dx + dySq > rEffSq) {
                    continue;
                }

                size_t idx = static_cast<size_t>(ty * width + tx);
                if (fogMap[idx] == static_cast<uint8_t>(FogState::Visible)) {
                    continue; // Đã nhìn thấy bởi đồng đội khác
                }

                int h1 = elevationGrid ? elevationGrid[idx] : 0;
                if (isLineOfSightClear(ux, uy, h0, tx, ty, h1, elevationGrid, width, airElevation)) {
                    fogMap[idx] = static_cast<uint8_t>(FogState::Visible);
                    if (outVisibleIndices) {
                        outVisibleIndices->push_back(static_cast<uint16_t>(idx));
                    }
                }
            }
        }
    }
}

void VisionSystem::updateFactionVision(Faction faction,
                                       const std::vector<Unit>& units,
                                       const WorldMap& map,
                                       const SimConfig& config,
                                       std::vector<uint8_t>& fogMap) {
    // Overload tiện ích: tạo mảng độ cao nếu chưa có
    int w = map.getWidth();
    int h = map.getHeight();
    std::vector<uint8_t> grid(w * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            DetailedTerrainType dt = config.classifyTerrain(map, x, y);
            grid[y * w + x] = static_cast<uint8_t>(config.getTerrainProperty(dt).elevation);
        }
    }
    updateFactionVision(faction, units, map, config, grid.data(), fogMap);
}

bool VisionSystem::isUnitVisibleToFaction(const Unit& targetUnit,
                                        Faction observerFaction,
                                        const std::vector<Unit>& allUnits,
                                        const std::vector<uint8_t>& observerFogMap,
                                        int mapWidth, int mapHeight) {
    if (targetUnit.isDead()) {
        return false;
    }

    if (targetUnit.getFaction() == observerFaction) {
        return true;
    }

    if (targetUnit.getType() == UnitType::Commander) {
        float stealthRangeTiles = targetUnit.getStealthRevealRangeTiles();
        if (stealthRangeTiles <= 0.0f) stealthRangeTiles = 1.0f;
        int64_t revealRangeMilli = static_cast<int64_t>(std::round(stealthRangeTiles * 1000.0f));
        int64_t revealDistSq = revealRangeMilli * revealRangeMilli;

        bool detectedByProximity = false;
        for (const auto& observer : allUnits) {
            if (observer.isDead() || observer.getFaction() != observerFaction) continue;
            int64_t ddx = static_cast<int64_t>(targetUnit.getPosX()) - observer.getPosX();
            int64_t ddy = static_cast<int64_t>(targetUnit.getPosY()) - observer.getPosY();
            if (ddx * ddx + ddy * ddy <= revealDistSq) {
                detectedByProximity = true;
                break;
            }
        }

        if (detectedByProximity) {
            return true;
        }

        if (targetUnit.isRevealed()) {
            int tx = targetUnit.getTileX();
            int ty = targetUnit.getTileY();
            if (tx >= 0 && tx < mapWidth && ty >= 0 && ty < mapHeight) {
                size_t idx = static_cast<size_t>(ty * mapWidth + tx);
                if (idx < observerFogMap.size() && observerFogMap[idx] == static_cast<uint8_t>(FogState::Visible)) {
                    return true;
                }
            }
        }

        return false;
    }

    if (targetUnit.getDomain() == UnitDomain::Air) {
        int tx = targetUnit.getTileX();
        int ty = targetUnit.getTileY();

        if (tx >= 0 && tx < mapWidth && ty >= 0 && ty < mapHeight) {
            size_t idx = static_cast<size_t>(ty * mapWidth + tx);
            if (idx < observerFogMap.size() && observerFogMap[idx] == static_cast<uint8_t>(FogState::Visible)) {
                return true;
            }
        }

        for (const auto& observer : allUnits) {
            if (observer.isDead() || observer.getFaction() != observerFaction) continue;
            int ox = observer.getTileX();
            int oy = observer.getTileY();
            int dx = tx - ox;
            int dy = ty - oy;
            int rEff = observer.getVisionTiles();
            if (dx * dx + dy * dy <= rEff * rEff) {
                return true;
            }
        }
        return false;
    }

    int tx = targetUnit.getTileX();
    int ty = targetUnit.getTileY();
    if (tx < 0 || tx >= mapWidth || ty < 0 || ty >= mapHeight) {
        return false;
    }

    size_t idx = static_cast<size_t>(ty * mapWidth + tx);
    if (idx >= observerFogMap.size()) {
        return false;
    }

    return observerFogMap[idx] == static_cast<uint8_t>(FogState::Visible);
}

} // namespace sim
