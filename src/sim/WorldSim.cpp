#include "WorldSim.hpp"
#include "vision/VisionSystem.hpp"
#include "../mapgen/FastRng.hpp"

#include <iostream>
#include <queue>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <chrono>

namespace sim {

WorldSim::WorldSim(WorldMap& map)
    : m_map(map) {}

bool WorldSim::loadConfig() {
    return m_config.autoLoad();
}

void WorldSim::queueCommand(const Command& cmd) {
    if (m_commandHook) {
        m_commandHook(m_currentTick, cmd);
    }
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
    m_activeTracers.clear();
    m_visiblePlayerIndices.clear();
    m_visibleEnemyIndices.clear();
    m_aliveIdsScratch.clear();
    m_matchEndInfo = MatchEndInfo{};
    m_currentTick = 0;
    m_combatRng = mapgen::FastRng(seed ^ 0xFEEDFACECAFEBEEFULL);

    int width = m_map.getWidth();
    int height = m_map.getHeight();
    int halfWidth = width / 2; // 64
    int midY = height / 2;     // 48

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
        UnitType::Commander,    // 0: 1 Commander
        UnitType::Infantry,     // 1..10: 10 Infantry
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Infantry,
        UnitType::Leopard2A,    // 11..12: 2 Leopard 2A
        UnitType::Leopard2A,
        UnitType::T14Armata,    // 13..14: 2 T-14 Armata
        UnitType::T14Armata,
        UnitType::Puma,         // 15..18: 4 Puma
        UnitType::Puma,
        UnitType::Puma,
        UnitType::Puma,
        UnitType::Caesar,       // 19..20: 2 Caesar
        UnitType::Caesar,
        UnitType::Gepard,       // 21..22: 2 Gepard
        UnitType::Gepard,
        UnitType::Su57,         // 23..24: 2 Su-57
        UnitType::Su57,
        UnitType::B2Spirit,     // 25: 1 B-2 Spirit
        UnitType::Mogami,       // 26..27: 2 Mogami (SEA)
        UnitType::Mogami,
        UnitType::Asagiri,      // 28..29: 2 Asagiri (SEA)
        UnitType::Asagiri
    };

    // Bố cục đội hình tương đối (relX, relY) theo điểm neo bờ biển (anchorX, anchorY)
    // Đối với Player (bên Trái): relX > 0 là tiến về phía Đông (hướng tâm bản đồ).
    // - Tuyến sau (Rearguard, sát bờ): Commander, Pháo Caesar, Pháo Gepard
    // - Tuyến giữa (Midline): Tăng hạng nặng Leopard 2A, T-14 Armata, Bộ binh (10 sư đoàn)
    // - Tuyến trước (Vanguard): 4 xe trinh sát Puma
    // - Thủy quân (Sea): ngoài biển sau lưng bờ (relX < 0)
    // - Không quân (Air): không phận phía trên điểm neo (relY < 0)
    struct FormationOffset {
        int relX{0};
        int relY{0};
    };

    const std::vector<FormationOffset> formationOffsets = {
        // 0: Commander (Tuyến sau, trung tâm đội hình)
        {0, 0},
        // 1..10: 10 Infantry (Tuyến giữa bộ binh, 2 hàng dọc 5 sư đoàn mỗi hàng)
        {3, -2}, {3, -1}, {3, 0}, {3, 1}, {3, 2},
        {4, -2}, {4, -1}, {4, 0}, {4, 1}, {4, 2},
        // 11..12: 2 Leopard 2A (Tăng hạng nặng ở cột 2)
        {2, -1}, {2, -2},
        // 13..14: 2 T-14 Armata (Tăng hạng trung ở cột 2)
        {2, 1}, {2, 2},
        // 15..18: 4 Puma (Xe trinh sát cơ động ở cột tiền tuyến 5)
        {5, -2}, {5, -1}, {5, 1}, {5, 2},
        // 19..20: 2 Caesar (Pháo tầm xa ở tuyến sau, hai bên Commander)
        {0, -2}, {0, 2},
        // 21..22: 2 Gepard (Phòng không bảo vệ tuyến sau)
        {0, -3}, {0, 3},
        // 23..24: 2 Su-57 (Không quân tiêm kích)
        {2, -4}, {3, -4},
        // 25: 1 B-2 Spirit (Oanh tạc cơ)
        {1, -4},
        // 26..27: 2 Mogami (Thủy quân ngoài biển phía sau bờ)
        {-3, -1}, {-3, -2},
        // 28..29: 2 Asagiri (Thủy quân ngoài biển phía sau bờ)
        {-3, 1}, {-3, 2}
    };

    // 1. Kiểm tra kết nối với khối lục địa chính (tránh đảo nhỏ cô lập giữa biển)
    auto isConnectedToMainLand = [&](int startX, int startY, int minTiles = 25) -> bool {
        std::vector<bool> visited(static_cast<size_t>(width * height), false);
        std::queue<Coord> q;
        q.push({startX, startY});
        visited[static_cast<size_t>(startY * width + startX)] = true;
        int count = 0;

        constexpr int ddx[4] = {1, -1, 0, 0};
        constexpr int ddy[4] = {0, 0, 1, -1};

        while (!q.empty()) {
            Coord c = q.front();
            q.pop();
            count++;
            if (count >= minTiles) return true;

            for (int i = 0; i < 4; ++i) {
                int nx = c.x + ddx[i];
                int ny = c.y + ddy[i];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                size_t idx = static_cast<size_t>(ny * width + nx);
                if (visited[idx]) continue;
                visited[idx] = true;

                DetailedTerrainType dt = m_config.classifyTerrain(m_map, nx, ny);
                if (m_config.isPassable(dt, UnitDomain::Land, UnitType::Infantry) &&
                    m_config.isPassable(dt, UnitDomain::Land, UnitType::Leopard2A)) {
                    q.push({nx, ny});
                }
            }
        }
        return count >= minTiles;
    };

    // 2. Tìm điểm bờ biển phía Tây (Player)
    auto findCoastAnchorWest = [&](int targetY) -> Coord {
        for (int x = 2; x < halfWidth - 5; ++x) {
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, x, targetY);
            if (m_config.isPassable(dt, UnitDomain::Land, UnitType::Infantry) &&
                m_config.isPassable(dt, UnitDomain::Land, UnitType::Leopard2A)) {
                if (isConnectedToMainLand(x, targetY, 25)) {
                    return {x, targetY};
                }
            }
        }
        return {-1, -1};
    };

    // 3. Tìm điểm bờ biển phía Đông (Enemy)
    auto findCoastAnchorEast = [&](int targetY) -> Coord {
        for (int x = width - 3; x >= halfWidth + 5; --x) {
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, x, targetY);
            if (m_config.isPassable(dt, UnitDomain::Land, UnitType::Infantry) &&
                m_config.isPassable(dt, UnitDomain::Land, UnitType::Leopard2A)) {
                if (isConnectedToMainLand(x, targetY, 25)) {
                    return {x, targetY};
                }
            }
        }
        return {-1, -1};
    };

    Coord playerAnchor{-1, -1};
    Coord enemyAnchor{-1, -1};

    // Ưu tiên 1: Tìm hàng cy chung gần midY nhất mà cả 2 bên đều có bờ biển đẹp
    for (int absDy = 0; absDy < height / 2; ++absDy) {
        int candYs[2] = {midY + absDy, midY - absDy};
        int countCand = (absDy == 0) ? 1 : 2;

        for (int i = 0; i < countCand; ++i) {
            int cy = candYs[i];
            if (cy < 4 || cy >= height - 4) continue;

            Coord pCand = findCoastAnchorWest(cy);
            Coord eCand = findCoastAnchorEast(cy);

            if (pCand.x != -1 && eCand.x != -1) {
                playerAnchor = pCand;
                enemyAnchor = eCand;
                break;
            }
        }
        if (playerAnchor.x != -1) break;
    }

    // Ưu tiên 2: Tìm độc lập hàng gần midY nhất cho từng bên nếu không có cy chung
    if (playerAnchor.x == -1) {
        for (int absDy = 0; absDy < height / 2; ++absDy) {
            int candYs[2] = {midY + absDy, midY - absDy};
            int countCand = (absDy == 0) ? 1 : 2;
            for (int i = 0; i < countCand; ++i) {
                int cy = candYs[i];
                if (cy < 4 || cy >= height - 4) continue;
                Coord pCand = findCoastAnchorWest(cy);
                if (pCand.x != -1) { playerAnchor = pCand; break; }
            }
            if (playerAnchor.x != -1) break;
        }
    }

    if (enemyAnchor.x == -1) {
        for (int absDy = 0; absDy < height / 2; ++absDy) {
            int candYs[2] = {midY + absDy, midY - absDy};
            int countCand = (absDy == 0) ? 1 : 2;
            for (int i = 0; i < countCand; ++i) {
                int cy = candYs[i];
                if (cy < 4 || cy >= height - 4) continue;
                Coord eCand = findCoastAnchorEast(cy);
                if (eCand.x != -1) { enemyAnchor = eCand; break; }
            }
            if (enemyAnchor.x != -1) break;
        }
    }

    // Fallback an toàn tuyệt đối nếu bản đồ dị biệt
    if (playerAnchor.x == -1) playerAnchor = {10, midY};
    if (enemyAnchor.x == -1) enemyAnchor = {width - 11, midY};

    m_playerSpawnAnchorX = playerAnchor.x;
    m_playerSpawnAnchorY = playerAnchor.y;
    m_enemySpawnAnchorX = enemyAnchor.x;
    m_enemySpawnAnchorY = enemyAnchor.y;

    // 4. Hàm tìm kiếm ô hợp lệ gần vị trí lý tưởng nhất
    auto findBestTile = [&](int idealX, int idealY, UnitDomain domain, UnitType type,
                            std::unordered_set<Coord, CoordHash>& occupied, bool isEastSide) -> Coord {
        int minX = isEastSide ? halfWidth : 0;
        int maxX = isEastSide ? (width - 1) : (halfWidth - 1);

        auto isValid = [&](int x, int y) {
            if (x < minX || x > maxX || y < 1 || y >= height - 1) return false;
            if (occupied.find(Coord{x, y}) != occupied.end()) return false;
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, x, y);
            return m_config.isPassable(dt, domain, type);
        };

        if (isValid(idealX, idealY)) {
            return Coord{idealX, idealY};
        }

        // Tìm kiếm xoắn ốc quanh idealX, idealY với bán kính tăng dần
        for (int r = 1; r < 35; ++r) {
            int bestX = -1;
            int bestY = -1;
            int bestDistSq = std::numeric_limits<int>::max();

            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                    int nx = idealX + dx;
                    int ny = idealY + dy;
                    if (isValid(nx, ny)) {
                        int dSq = dx * dx + dy * dy;
                        if (dSq < bestDistSq) {
                            bestDistSq = dSq;
                            bestX = nx;
                            bestY = ny;
                        }
                    }
                }
            }
            if (bestX != -1) {
                return Coord{bestX, bestY};
            }
        }

        // Fallback quét toàn bộ nửa sân
        for (int y = 1; y < height - 1; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (isValid(x, y)) {
                    return Coord{x, y};
                }
            }
        }
        return Coord{idealX, idealY};
    };

    // 5. Xác định tọa độ 30 đơn vị cho Player
    std::vector<Coord> playerTiles;
    std::unordered_set<Coord, CoordHash> occupiedPlayer;

    for (size_t i = 0; i < roster.size(); ++i) {
        UnitType t = roster[i];
        const auto* cfg = m_config.getUnitConfig(t);
        UnitDomain domain = cfg ? cfg->domain : UnitDomain::Land;

        int idealX = playerAnchor.x + formationOffsets[i].relX;
        int idealY = playerAnchor.y + formationOffsets[i].relY;

        Coord assigned = findBestTile(idealX, idealY, domain, t, occupiedPlayer, false);
        playerTiles.push_back(assigned);
        occupiedPlayer.insert(assigned);
    }

    // 6. Xác định tọa độ 30 đơn vị cho Enemy (đối xứng gương qua trung điểm bờ Đông)
    std::vector<Coord> enemyTiles;
    std::unordered_set<Coord, CoordHash> occupiedEnemy;

    for (size_t i = 0; i < roster.size(); ++i) {
        UnitType t = roster[i];
        const auto* cfg = m_config.getUnitConfig(t);
        UnitDomain domain = cfg ? cfg->domain : UnitDomain::Land;

        int idealX = enemyAnchor.x - formationOffsets[i].relX;
        int idealY = enemyAnchor.y + formationOffsets[i].relY;

        Coord assigned = findBestTile(idealX, idealY, domain, t, occupiedEnemy, true);
        enemyTiles.push_back(assigned);
        occupiedEnemy.insert(assigned);
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

    // Khởi tạo và cập nhật sương mù chiến tranh ban đầu ở tick 0 (M6)
    size_t mapSize = static_cast<size_t>(m_map.getWidth() * m_map.getHeight());
    m_fogPlayer.assign(mapSize, static_cast<uint8_t>(FogState::Unexplored));
    m_fogEnemy.assign(mapSize, static_cast<uint8_t>(FogState::Unexplored));
    updateVision();
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
    // Nếu trận đấu đã kết thúc, dừng toàn bộ mô phỏng
    if (isGameOver()) {
        return;
    }

    // 1. Áp dụng các lệnh từ hàng đợi ở đầu tick
    for (const auto& cmd : m_commandQueue) {
        if (cmd.type == CommandType::Move) {
            std::vector<Unit*> targetUnits;
            for (uint32_t uid : cmd.move.unitIds) {
                Unit* unit = getUnitById(uid);
                if (unit && !unit->isDead()) {
                    targetUnits.push_back(unit);
                }
            }

            if (!targetUnits.empty()) {
                TileCoord centerTarget{cmd.move.targetTileX, cmd.move.targetTileY};

                // Rải các ô đích quanh centerTarget cho số lượng đơn vị
                auto dispersedTargets = Pathfinder::disperseDestinations(
                    m_map, m_config, targetUnits[0]->getDomain(), targetUnits[0]->getType(),
                    centerTarget, targetUnits.size()
                );

                for (size_t i = 0; i < targetUnits.size(); ++i) {
                    Unit* unit = targetUnits[i];
                    TileCoord assignedTarget = (i < dispersedTargets.size()) ? dispersedTargets[i] : centerTarget;

                    DetailedTerrainType dtAssigned = getDetailedTerrain(assignedTarget.x, assignedTarget.y);
                    if (!m_config.isPassable(dtAssigned, unit->getDomain(), unit->getType())) {
                        auto specificDispersed = Pathfinder::disperseDestinations(
                            m_map, m_config, unit->getDomain(), unit->getType(), centerTarget, 1
                        );
                        if (!specificDispersed.empty()) {
                            assignedTarget = specificDispersed[0];
                        }
                    }

                    if (unit->getTileX() == assignedTarget.x && unit->getTileY() == assignedTarget.y) {
                        unit->clearPath();
                        unit->setLastPathfinderResult(true, true, 0);
                        continue;
                    }

                    if (unit->hasPath() && unit->getFinalTargetTile() == assignedTarget) {
                        continue;
                    }

                    TileCoord startCoord{unit->getTileX(), unit->getTileY()};
                    PathResult path = Pathfinder::findPath(
                        m_map, m_config, unit->getDomain(), unit->getType(), startCoord, assignedTarget
                    );

                    unit->setLastPathfinderResult(true, path.found, static_cast<int>(path.waypoints.size()));

                    if (path.found) {
                        unit->setPath(path);
                    }
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
        DetailedTerrainType currentTerrain = getDetailedTerrain(unit.getTileX(), unit.getTileY());

        // Tính quãng đường di chuyển trong tick này
        int stepMilli = m_config.calculateStepMilli(currentTerrain, unit.getType(), unit.getMoveAccumulator());

        if (stepMilli > 0) {
            unit.stepTowardsDestination(stepMilli);
        }
    }

    // Giảm thời gian lộ diện của các đơn vị (M6)
    for (auto& unit : m_units) {
        if (!unit.isDead()) {
            unit.decrementRevealedTicks();
        }
    }

    // 3. Cập nhật tầm nhìn và sương mù chiến tranh cho cả hai phe (M6)
    updateVision();

    // 4. Tự động giao chiến và bắn phá (M5)
    resolveCombat();

    // 5. Quy tắc M3: Chấm hết máu (isDead) thì chấm mất khỏi bản đồ
    m_units.erase(
        std::remove_if(m_units.begin(), m_units.end(), [](const Unit& u) {
            return u.isDead();
        }),
        m_units.end()
    );

    // 6. Kiểm tra điều kiện thắng thua / hết giờ (M5)
    checkMatchConditions();

    m_currentTick++;
}

void WorldSim::resolveCombat() {
    // Nếu sương mù chưa từng được tính (ví dụ trong unit test gọi thẳng resolveCombat), cập nhật vision trước
    if (m_fogPlayer.empty() || m_fogEnemy.empty()) {
        updateVision();
    }

    // 1. Dọn dẹp các đường đạn cũ (giữ lại các đường đạn trong 3 tick gần nhất)
    m_activeTracers.erase(
        std::remove_if(m_activeTracers.begin(), m_activeTracers.end(), [this](const Tracer& t) {
            return (m_currentTick >= t.spawnTick + 3);
        }),
        m_activeTracers.end()
    );

    if (isGameOver()) return;

    // 2. Duyệt các chấm còn sống theo thứ tự id cố định tăng dần để đảm bảo tính tất định
    m_aliveIdsScratch.clear();
    m_aliveIdsScratch.reserve(m_units.size());
    for (const auto& u : m_units) {
        if (!u.isDead()) {
            m_aliveIdsScratch.push_back(u.getId());
        }
    }
    std::sort(m_aliveIdsScratch.begin(), m_aliveIdsScratch.end());

    for (uint32_t attackerId : m_aliveIdsScratch) {
        Unit* attacker = getUnitById(attackerId);
        if (!attacker || attacker->isDead()) continue;

        if (attacker->getCooldownRemaining() > 0) {
            attacker->decrementCooldown();
            continue;
        }

        if (attacker->getWeapons().empty()) continue;

        // Tìm mục tiêu địch gần nhất trong tầm đánh
        Unit* bestTarget = nullptr;
        int64_t bestDistSq = std::numeric_limits<int64_t>::max();

        for (auto& candidate : m_units) {
            if (candidate.isDead()) continue;
            if (!CombatSystem::canAttackTarget(*attacker, candidate)) continue;
            // M6: Chỉ nhắm mục tiêu đang được phe mình nhìn thấy
            if (!isUnitVisible(candidate, attacker->getFaction())) continue;

            int64_t dx = static_cast<int64_t>(candidate.getPosX()) - attacker->getPosX();
            int64_t dy = static_cast<int64_t>(candidate.getPosY()) - attacker->getPosY();
            int64_t dSq = dx * dx + dy * dy;

            if (dSq < bestDistSq || (dSq == bestDistSq && candidate.getId() < (bestTarget ? bestTarget->getId() : 99999999U))) {
                bestDistSq = dSq;
                bestTarget = &candidate;
            }
        }

        if (!bestTarget) continue;

        // Chọn vũ khí tối ưu cho tổng sát thương lớn nhất lên mục tiêu
        size_t wIdx = CombatSystem::chooseBestWeaponIndex(
            attacker->getType(),
            attacker->getWeapons(),
            attacker->getCurrentUnits(),
            bestTarget->getArmor(),
            bestTarget->getDomain()
        );
        const auto& weapon = attacker->getWeapons()[wIdx];

        // Tính số phát trúng (Accuracy) bằng bộ sinh số ngẫu nhiên của sim (tất định theo seed)
        int aliveUnits = attacker->getCurrentUnits();
        int64_t totalShots = static_cast<int64_t>(aliveUnits) * weapon.rounds;
        int64_t accPercent = static_cast<int64_t>(std::round(weapon.accuracy * 100.0));
        int64_t hits = 0;
        if (accPercent >= 100) {
            hits = totalShots;
        } else if (accPercent <= 0) {
            hits = 0;
        } else {
            int64_t rawHits100 = totalShots * accPercent;
            int64_t baseHits = rawHits100 / 100;
            int64_t rem = rawHits100 % 100;
            if (rem > 0 && m_combatRng.range(0, 99) < rem) {
                baseHits++;
            }
            hits = baseHits;
        }

        // Tính và trừ sát thương lên mục tiêu chính
        int64_t mainDmg = CombatSystem::calculateVolleyDamage(
            attacker->getType(),
            weapon,
            aliveUnits,
            bestTarget->getArmor(),
            bestTarget->getDomain(),
            false,
            static_cast<int>(hits)
        );
        bestTarget->takeDamage(static_cast<int>(mainDmg));

        // M6: Tư lệnh lộ diện sau khi nổ súng
        if (attacker->getType() == UnitType::Commander) {
            attacker->setRevealedTicks(m_config.getVisionConfig().commanderRevealTicks);
        }

        // Lưu đường đạn để hiển thị
        Tracer tr;
        tr.startPosX = attacker->getPosX();
        tr.startPosY = attacker->getPosY();
        tr.endPosX = bestTarget->getPosX();
        tr.endPosY = bestTarget->getPosY();
        tr.spawnTick = m_currentTick;
        tr.ammoType = weapon.ammo;
        m_activeTracers.push_back(tr);

        // Đạn nổ có splash: gây thêm 55% sát thương tính độc lập cho các chấm địch xung quanh
        if (weapon.splash > 0.0f) {
            int64_t splashRadiusMilli = static_cast<int64_t>(weapon.splash * 1000.0f);
            int64_t splashRadiusSq = splashRadiusMilli * splashRadiusMilli;

            for (auto& other : m_units) {
                if (other.isDead() || other.getId() == bestTarget->getId()) continue;
                // Chỉ gây sát thương cho quân địch của kẻ tấn công (cùng phe với bestTarget)
                if (other.getFaction() != bestTarget->getFaction()) continue;

                int64_t dx = static_cast<int64_t>(other.getPosX()) - bestTarget->getPosX();
                int64_t dy = static_cast<int64_t>(other.getPosY()) - bestTarget->getPosY();
                int64_t dSq = dx * dx + dy * dy;

                if (dSq <= splashRadiusSq) {
                    int64_t splashDmg = CombatSystem::calculateVolleyDamage(
                        attacker->getType(),
                        weapon,
                        aliveUnits,
                        other.getArmor(),
                        other.getDomain(),
                        true,
                        static_cast<int>(hits),
                        550
                    );
                    other.takeDamage(static_cast<int>(splashDmg));
                }
            }
        }

        // Đặt lại cooldown
        attacker->resetCooldown();
    }
}

void WorldSim::checkMatchConditions() {
    if (isGameOver()) return;

    bool playerCommanderAlive = false;
    bool enemyCommanderAlive = false;
    int playerOtherCount = 0;
    int enemyOtherCount = 0;
    int playerTotalUnits = 0;
    int enemyTotalUnits = 0;

    for (const auto& u : m_units) {
        if (u.isDead()) continue;

        if (u.getFaction() == Faction::Player) {
            if (u.getType() == UnitType::Commander) {
                playerCommanderAlive = true;
            } else {
                playerOtherCount++;
            }
            playerTotalUnits += u.getCurrentUnits();
        } else if (u.getFaction() == Faction::Enemy) {
            if (u.getType() == UnitType::Commander) {
                enemyCommanderAlive = true;
            } else {
                enemyOtherCount++;
            }
            enemyTotalUnits += u.getCurrentUnits();
        }
    }

    // 1. Cả hai tư lệnh đều chết hoặc hai bên đều hết quân
    if (!playerCommanderAlive && !enemyCommanderAlive) {
        m_matchEndInfo.result = MatchResult::Draw;
        m_matchEndInfo.kind = MatchEndKind::Draw;
        m_matchEndInfo.reason = "Tu lenh hai ben deu da tu tran! Hoa tran.";
        return;
    }
    if (playerTotalUnits == 0 && enemyTotalUnits == 0) {
        m_matchEndInfo.result = MatchResult::Draw;
        m_matchEndInfo.kind = MatchEndKind::Draw;
        m_matchEndInfo.reason = "Quan luc hai ben deu da bi tieu diet sach! Hoa tran.";
        return;
    }

    // 2. Thua khi tư lệnh chết
    if (!playerCommanderAlive) {
        m_matchEndInfo.result = MatchResult::Defeat;
        m_matchEndInfo.kind = MatchEndKind::CommanderKilled;
        m_matchEndInfo.reason = "Tu lenh cua ban da hy sinh! That bai.";
        return;
    }

    // 3. Thắng khi tư lệnh địch chết
    if (!enemyCommanderAlive) {
        m_matchEndInfo.result = MatchResult::Victory;
        m_matchEndInfo.kind = MatchEndKind::CommanderKilled;
        m_matchEndInfo.reason = "Tu lenh dich da bi tieu diet! Chien thang!";
        return;
    }

    // 4. Thắng khi địch hết quân (chỉ còn tư lệnh)
    if (enemyOtherCount == 0 && playerOtherCount > 0) {
        m_matchEndInfo.result = MatchResult::Victory;
        m_matchEndInfo.kind = MatchEndKind::TroopsWiped;
        m_matchEndInfo.reason = "Quan dich da bi tieu diet hoan toan (chi con tu lenh)! Chien thang!";
        return;
    }

    // 5. Cả 2 bên đều chỉ còn tư lệnh (hết quân)
    if (enemyOtherCount == 0 && playerOtherCount == 0) {
        m_matchEndInfo.result = MatchResult::Draw;
        m_matchEndInfo.kind = MatchEndKind::Draw;
        m_matchEndInfo.reason = "Quan luc hai ben deu da bi tieu diet sach! Hoa tran.";
        return;
    }

    // 6. Hết giờ: bên nào có tổng quân gộp nhiều hơn thắng; bằng nhau thì hòa
    if (m_currentTick >= getMatchDurationTicks()) {
        if (playerTotalUnits > enemyTotalUnits) {
            m_matchEndInfo.result = MatchResult::Victory;
            m_matchEndInfo.kind = MatchEndKind::Timeout;
            m_matchEndInfo.reason = "Het thoi gian! Phe ban thang nho tong quan so ap dao (" +
                                    std::to_string(playerTotalUnits) + " vs " + std::to_string(enemyTotalUnits) + ").";
        } else if (enemyTotalUnits > playerTotalUnits) {
            m_matchEndInfo.result = MatchResult::Defeat;
            m_matchEndInfo.kind = MatchEndKind::Timeout;
            m_matchEndInfo.reason = "Het thoi gian! Phe dich thang nho tong quan so ap dao (" +
                                    std::to_string(enemyTotalUnits) + " vs " + std::to_string(playerTotalUnits) + ").";
        } else {
            m_matchEndInfo.result = MatchResult::Draw;
            m_matchEndInfo.kind = MatchEndKind::Draw;
            m_matchEndInfo.reason = "Het thoi gian! Quan so hai ben can bang (" +
                                    std::to_string(playerTotalUnits) + "). Hoa tran.";
        }
    }
}

int WorldSim::getRemainingTimeSec() const {
    int totalSec = m_config.getMatchDurationSec();
    int tps = m_config.getTickRate();
    if (tps <= 0) tps = 20;
    int elapsedSec = static_cast<int>(m_currentTick / tps);
    return std::max(0, totalSec - elapsedSec);
}

void WorldSim::initElevationGrid() {
    int w = m_map.getWidth();
    int h = m_map.getHeight();
    size_t total = static_cast<size_t>(w * h);
    m_elevationGrid.resize(total);
    m_detailedTerrainGrid.resize(total);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            DetailedTerrainType dt = m_config.classifyTerrain(m_map, x, y);
            size_t idx = static_cast<size_t>(y * w + x);
            m_detailedTerrainGrid[idx] = static_cast<uint8_t>(dt);
            m_elevationGrid[idx] = static_cast<uint8_t>(m_config.getTerrainProperty(dt).elevation);
        }
    }
}

DetailedTerrainType WorldSim::getDetailedTerrain(int x, int y) const {
    if (!m_map.isInBounds(x, y)) return DetailedTerrainType::DeepSea;
    if (m_detailedTerrainGrid.empty()) return m_config.classifyTerrain(m_map, x, y);
    return static_cast<DetailedTerrainType>(m_detailedTerrainGrid[static_cast<size_t>(y * m_map.getWidth() + x)]);
}

void WorldSim::updateVision() {
    if (m_elevationGrid.empty()) {
        initElevationGrid();
    }

    VisionSystem::updateFactionVision(Faction::Player, m_units, m_map, m_config, m_elevationGrid.data(), m_fogPlayer, &m_visiblePlayerIndices);
    VisionSystem::updateFactionVision(Faction::Enemy, m_units, m_map, m_config, m_elevationGrid.data(), m_fogEnemy, &m_visibleEnemyIndices);
}

bool WorldSim::isTileVisible(int x, int y, Faction f) const {
    if (!m_map.isInBounds(x, y)) return false;
    const auto& fog = getFogMap(f);
    size_t idx = static_cast<size_t>(y * m_map.getWidth() + x);
    if (idx >= fog.size()) return false;
    return fog[idx] == static_cast<uint8_t>(FogState::Visible);
}

bool WorldSim::isUnitVisible(const Unit& u, Faction observerFaction) const {
    if (m_fogPlayer.empty() || m_fogEnemy.empty()) {
        const_cast<WorldSim*>(this)->updateVision();
    }
    const auto& fog = getFogMap(observerFaction);
    return VisionSystem::isUnitVisibleToFaction(u, observerFaction, m_units, fog, m_map.getWidth(), m_map.getHeight());
}

} // namespace sim
