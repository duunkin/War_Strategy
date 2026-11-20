#include "WorldSim.hpp"
#include "../mapgen/FastRng.hpp"

#include <iostream>
#include <queue>
#include <vector>
#include <algorithm>
#include <unordered_set>

namespace sim {

WorldSim::WorldSim(WorldMap& map)
    : m_map(map) {}

bool WorldSim::loadConfig() {
    return m_config.autoLoad();
}

void WorldSim::queueCommand(const Command& cmd) {
    m_commandQueue.push_back(cmd);
}

void WorldSim::queueMoveCommand(const std::vector<uint32_t>& unitIds, int targetTileX, int targetTileY) {
    Command cmd;
    cmd.type = CommandType::Move;
    cmd.move.unitIds = unitIds;
    cmd.move.targetTileX = targetTileX;
    cmd.move.targetTileY = targetTileY;
    queueCommand(cmd);
}

void WorldSim::spawnDivisions(uint64_t seed) {
    m_units.clear();
    m_commandQueue.clear();

    int width = m_map.getWidth();
    int height = m_map.getHeight();
    int halfWidth = width / 2; // 64

    mapgen::FastRng rng(seed ^ 0xFEEDFACECAFEBEEFULL);

    struct Coord {
        int x{0};
        int y{0};
        bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    };

    struct CoordHash {
        size_t operator()(const Coord& c) const {
            return static_cast<size_t>(c.y * 1000 + c.x);
        }
    };

    // Danh sách 30 đơn vị mỗi bên theo đúng JSON
    const std::vector<UnitType> roster = {
        UnitType::Commander,    // 1 Commander
        UnitType::Infantry,     // 10 Infantry
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Leopard2A,    // 2 Leopard 2A
        UnitType::Leopard2A,
        UnitType::T14Armata,    // 2 T-14 Armata
        UnitType::T14Armata,
        UnitType::Puma,         // 4 Puma
        UnitType::Puma,
        UnitType::Puma,
        UnitType::Puma,
        UnitType::Caesar,       // 2 Caesar
        UnitType::Caesar,
        UnitType::Gepard,       // 2 Gepard
        UnitType::Gepard,
        UnitType::Su57,         // 2 Su-57
        UnitType::Su57,
        UnitType::B2Spirit,     // 1 B-2 Spirit
        UnitType::Mogami,       // 2 Mogami (SEA)
        UnitType::Mogami,
        UnitType::Asagiri,      // 2 Asagiri (SEA)
        UnitType::Asagiri
    };

    // 1. Phân loại 26 đơn vị trên bộ/trên không và 4 tàu biển
    std::vector<UnitType> landUnits;
    std::vector<UnitType> seaUnits;
    for (UnitType t : roster) {
        const auto* cfg = m_config.getUnitConfig(t);
        UnitDomain domain = cfg ? cfg->domain : UnitDomain::Land;
        if (domain == UnitDomain::Sea) {
            seaUnits.push_back(t);
        } else {
            landUnits.push_back(t);
        }
    }

    // 2. Tìm 26 ô đất liền hợp lệ cho Player ở phía Tây (x < halfWidth)
    std::vector<Coord> playerLandTiles;
    std::unordered_set<Coord, CoordHash> occupiedPlayer;

    // Tìm điểm neo đất bằng (Plain) ở Tây
    for (int attempt = 0; attempt < 500 && playerLandTiles.size() < landUnits.size(); ++attempt) {
        int anchorX = rng.range(8, halfWidth - 8);
        int anchorY = rng.range(12, height - 13);

        DetailedTerrainType dt = m_config.classifyTerrain(m_map, anchorX, anchorY);
        if (dt != DetailedTerrainType::Plain) {
            continue;
        }

        std::vector<bool> visited(static_cast<size_t>(width * height), false);
        std::queue<Coord> queue;
        std::vector<Coord> cluster;

        visited[static_cast<size_t>(anchorY * width + anchorX)] = true;
        queue.push({anchorX, anchorY});
        cluster.push_back({anchorX, anchorY});

        constexpr int dx[4] = {0, 1, 0, -1};
        constexpr int dy[4] = {-1, 0, 1, 0};

        while (!queue.empty() && cluster.size() < landUnits.size()) {
            Coord cur = queue.front();
            queue.pop();

            for (int dir = 0; dir < 4; ++dir) {
                int nx = cur.x + dx[dir];
                int ny = cur.y + dy[dir];

                if (nx < 2 || nx >= halfWidth - 2 || ny < 2 || ny >= height - 2) continue;
                size_t idx = static_cast<size_t>(ny * width + nx);
                if (visited[idx]) continue;
                visited[idx] = true;

                DetailedTerrainType ndt = m_config.classifyTerrain(m_map, nx, ny);
                // Ô đất liền đi được bởi bộ binh và xe (Plain/Forest/Hill)
                if (m_config.isPassable(ndt, UnitDomain::Land, UnitType::Infantry) &&
                    m_config.isPassable(ndt, UnitDomain::Land, UnitType::Leopard2A)) {
                    cluster.push_back({nx, ny});
                    queue.push({nx, ny});
                    if (cluster.size() >= landUnits.size()) break;
                }
            }
        }

        if (cluster.size() >= landUnits.size()) {
            playerLandTiles = std::move(cluster);
            break;
        }
    }

    // Nếu BFS chưa đủ, quét tuyến tính trên nửa Tây
    if (playerLandTiles.size() < landUnits.size()) {
        playerLandTiles.clear();
        for (int y = 8; y < height - 8 && playerLandTiles.size() < landUnits.size(); ++y) {
            for (int x = 6; x < halfWidth - 2 && playerLandTiles.size() < landUnits.size(); ++x) {
                DetailedTerrainType ndt = m_config.classifyTerrain(m_map, x, y);
                if (m_config.isPassable(ndt, UnitDomain::Land, UnitType::Infantry) &&
                    m_config.isPassable(ndt, UnitDomain::Land, UnitType::Leopard2A)) {
                    playerLandTiles.push_back({x, y});
                }
            }
        }
    }

    for (const auto& c : playerLandTiles) {
        occupiedPlayer.insert(c);
    }

    // 3. Tìm 4 ô biển cho tàu Player ở phía Tây (x < halfWidth)
    std::vector<Coord> playerSeaTiles;
    for (int y = 4; y < height - 4 && playerSeaTiles.size() < seaUnits.size(); ++y) {
        for (int x = 2; x < halfWidth && playerSeaTiles.size() < seaUnits.size(); ++x) {
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, x, y);
            if (m_config.isPassable(dt, UnitDomain::Sea, UnitType::Mogami)) {
                Coord c{x, y};
                if (occupiedPlayer.find(c) == occupiedPlayer.end()) {
                    playerSeaTiles.push_back(c);
                    occupiedPlayer.insert(c);
                }
            }
        }
    }

    // Ghép toàn bộ tọa độ Player theo thứ tự roster
    std::vector<Coord> playerTiles;
    size_t landIdx = 0;
    size_t seaIdx = 0;
    for (UnitType t : roster) {
        const auto* cfg = m_config.getUnitConfig(t);
        UnitDomain domain = cfg ? cfg->domain : UnitDomain::Land;
        if (domain == UnitDomain::Sea) {
            if (seaIdx < playerSeaTiles.size()) {
                playerTiles.push_back(playerSeaTiles[seaIdx++]);
            } else {
                playerTiles.push_back({2, 10});
            }
        } else {
            if (landIdx < playerLandTiles.size()) {
                playerTiles.push_back(playerLandTiles[landIdx++]);
            } else {
                playerTiles.push_back({10, 10});
            }
        }
    }

    // 4. Tìm tọa độ đối xứng gương cho Enemy ở phía Đông (x >= halfWidth)
    std::vector<Coord> enemyTiles;
    std::unordered_set<Coord, CoordHash> occupiedEnemy;

    auto findNearestPassableEast = [&](int idealX, int idealY, UnitDomain domain, UnitType type) -> Coord {
        // Kiểm tra ngay ô đối xứng gương
        if (idealX >= halfWidth && idealX < width && idealY >= 0 && idealY < height) {
            Coord cand{idealX, idealY};
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, idealX, idealY);
            if (m_config.isPassable(dt, domain, type) && occupiedEnemy.find(cand) == occupiedEnemy.end()) {
                return cand;
            }
        }

        // Tìm kiếm xoắn ốc / BFS mở rộng từ idealX, idealY trong phạm vi nửa Đông
        for (int radius = 1; radius < 40; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                    int nx = idealX + dx;
                    int ny = idealY + dy;
                    if (nx >= halfWidth && nx < width && ny >= 0 && ny < height) {
                        Coord cand{nx, ny};
                        if (occupiedEnemy.find(cand) != occupiedEnemy.end()) continue;
                        DetailedTerrainType dt = m_config.classifyTerrain(m_map, nx, ny);
                        if (m_config.isPassable(dt, domain, type)) {
                            return cand;
                        }
                    }
                }
            }
        }

        // Fallback quét tuyến tính nửa Đông
        for (int ny = 0; ny < height; ++ny) {
            for (int nx = halfWidth; nx < width; ++nx) {
                Coord cand{nx, ny};
                if (occupiedEnemy.find(cand) != occupiedEnemy.end()) continue;
                DetailedTerrainType dt = m_config.classifyTerrain(m_map, nx, ny);
                if (m_config.isPassable(dt, domain, type)) {
                    return cand;
                }
            }
        }
        return Coord{idealX, idealY};
    };

    for (size_t i = 0; i < roster.size(); ++i) {
        UnitType t = roster[i];
        const auto* cfg = m_config.getUnitConfig(t);
        UnitDomain domain = cfg ? cfg->domain : UnitDomain::Land;
        int idealX = width - 1 - playerTiles[i].x;
        int idealY = playerTiles[i].y;
        Coord eCoord = findNearestPassableEast(idealX, idealY, domain, t);
        enemyTiles.push_back(eCoord);
        occupiedEnemy.insert(eCoord);
    }

    // 5. Khởi tạo các đơn vị Unit cho Player (ID: 1..30)
    uint32_t nextId = 1;
    for (size_t i = 0; i < roster.size(); ++i) {
        UnitType t = roster[i];
        const auto* cfg = m_config.getUnitConfig(t);
        if (!cfg) continue;
        int32_t milliX = playerTiles[i].x * 1000 + 500;
        int32_t milliY = playerTiles[i].y * 1000 + 500;
        m_units.emplace_back(nextId++, Faction::Player, t, milliX, milliY, *cfg);
    }

    // 6. Khởi tạo các đơn vị Unit cho Enemy (ID: 31..60)
    for (size_t i = 0; i < roster.size(); ++i) {
        UnitType t = roster[i];
        const auto* cfg = m_config.getUnitConfig(t);
        if (!cfg) continue;
        int32_t milliX = enemyTiles[i].x * 1000 + 500;
        int32_t milliY = enemyTiles[i].y * 1000 + 500;
        m_units.emplace_back(nextId++, Faction::Enemy, t, milliX, milliY, *cfg);
    }

    std::cout << "[WorldSim] Da sinh thanh cong " << m_units.size() << " don vi ("
              << roster.size() << " Player phia Tay, " << roster.size() << " Enemy phia Dong doi xung) tu seed " << seed << std::endl;
}

const Unit* WorldSim::getUnitById(uint32_t id) const {
    for (const auto& u : m_units) {
        if (u.getId() == id) return &u;
    }
    return nullptr;
}

Unit* WorldSim::getUnitById(uint32_t id) {
    for (auto& u : m_units) {
        if (u.getId() == id) return &u;
    }
    return nullptr;
}

void WorldSim::tick() {
    // 1. Áp dụng các lệnh từ hàng đợi ở đầu tick
    for (const auto& cmd : m_commandQueue) {
        if (cmd.type == CommandType::Move) {
            for (uint32_t uid : cmd.move.unitIds) {
                Unit* unit = getUnitById(uid);
                if (unit && !unit->isDead()) {
                    unit->setDestinationTile(cmd.move.targetTileX, cmd.move.targetTileY);
                }
            }
        }
    }
    m_commandQueue.clear();

    // 2. Cập nhật di chuyển cho từng đơn vị còn sống
    for (auto& unit : m_units) {
        if (unit.isDead() || !unit.hasDestination()) {
            continue;
        }

        // Lấy địa hình chi tiết của ô đơn vị đang đứng
        DetailedTerrainType currentTerrain = m_config.classifyTerrain(m_map, unit.getTileX(), unit.getTileY());

        // Tính quãng đường di chuyển trong tick này
        int stepMilli = m_config.calculateStepMilli(currentTerrain, unit.getType(), unit.getMoveAccumulator());

        if (stepMilli > 0) {
            unit.stepTowardsDestination(stepMilli);
        }
    }

    // 3. Quy tắc M3: Chấm hết máu (isDead) thì chấm mất khỏi bản đồ
    m_units.erase(
        std::remove_if(m_units.begin(), m_units.end(), [](const Unit& u) {
            return u.isDead();
        }),
        m_units.end()
    );

    m_currentTick++;
}

} // namespace sim
