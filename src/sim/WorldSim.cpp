#include "WorldSim.hpp"
#include "../mapgen/FastRng.hpp"

#include <iostream>
#include <queue>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <limits>
#include <cmath>

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
    m_activeTracers.clear();
    m_matchEndInfo = MatchEndInfo{};
    m_currentTick = 0;
    m_combatRng = mapgen::FastRng(seed ^ 0xFEEDFACECAFEBEEFULL);

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

                    DetailedTerrainType dtAssigned = m_config.classifyTerrain(m_map, assignedTarget.x, assignedTarget.y);
                    if (!m_config.isPassable(dtAssigned, unit->getDomain(), unit->getType())) {
                        auto specificDispersed = Pathfinder::disperseDestinations(
                            m_map, m_config, unit->getDomain(), unit->getType(), centerTarget, 1
                        );
                        if (!specificDispersed.empty()) {
                            assignedTarget = specificDispersed[0];
                        }
                    }

                    TileCoord startCoord{unit->getTileX(), unit->getTileY()};
                    PathResult path = Pathfinder::findPath(
                        m_map, m_config, unit->getDomain(), unit->getType(), startCoord, assignedTarget
                    );

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
        DetailedTerrainType currentTerrain = m_config.classifyTerrain(m_map, unit.getTileX(), unit.getTileY());

        // Tính quãng đường di chuyển trong tick này
        int stepMilli = m_config.calculateStepMilli(currentTerrain, unit.getType(), unit.getMoveAccumulator());

        if (stepMilli > 0) {
            unit.stepTowardsDestination(stepMilli);
        }
    }

    // 3. Tự động giao chiến và bắn phá (M5)
    resolveCombat();

    // 4. Quy tắc M3: Chấm hết máu (isDead) thì chấm mất khỏi bản đồ
    m_units.erase(
        std::remove_if(m_units.begin(), m_units.end(), [](const Unit& u) {
            return u.isDead();
        }),
        m_units.end()
    );

    // 5. Kiểm tra điều kiện thắng thua / hết giờ (M5)
    checkMatchConditions();

    m_currentTick++;
}

void WorldSim::resolveCombat() {
    // 1. Dọn dẹp các đường đạn cũ (giữ lại các đường đạn trong 3 tick gần nhất)
    m_activeTracers.erase(
        std::remove_if(m_activeTracers.begin(), m_activeTracers.end(), [this](const Tracer& t) {
            return (m_currentTick >= t.spawnTick + 3);
        }),
        m_activeTracers.end()
    );

    if (isGameOver()) return;

    // 2. Duyệt các chấm còn sống theo thứ tự id cố định tăng dần để đảm bảo tính tất định
    std::vector<uint32_t> aliveIds;
    aliveIds.reserve(m_units.size());
    for (const auto& u : m_units) {
        if (!u.isDead()) {
            aliveIds.push_back(u.getId());
        }
    }
    std::sort(aliveIds.begin(), aliveIds.end());

    for (uint32_t attackerId : aliveIds) {
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
        m_matchEndInfo.reason = "Tu lenh hai ben deu da tu tran! Hoa tran.";
        return;
    }
    if (playerTotalUnits == 0 && enemyTotalUnits == 0) {
        m_matchEndInfo.result = MatchResult::Draw;
        m_matchEndInfo.reason = "Quan luc hai ben deu da bi tieu diet sach! Hoa tran.";
        return;
    }

    // 2. Thua khi tư lệnh chết
    if (!playerCommanderAlive) {
        m_matchEndInfo.result = MatchResult::Defeat;
        m_matchEndInfo.reason = "Tu lenh cua ban da hy sinh! That bai.";
        return;
    }

    // 3. Thắng khi tư lệnh địch chết
    if (!enemyCommanderAlive) {
        m_matchEndInfo.result = MatchResult::Victory;
        m_matchEndInfo.reason = "Tu lenh dich da bi tieu diet! Chien thang!";
        return;
    }

    // 4. Thắng khi địch hết quân (chỉ còn tư lệnh)
    if (enemyOtherCount == 0 && playerOtherCount > 0) {
        m_matchEndInfo.result = MatchResult::Victory;
        m_matchEndInfo.reason = "Quan dich da bi tieu diet hoan toan (chi con tu lenh)! Chien thang!";
        return;
    }

    // 5. Cả 2 bên đều chỉ còn tư lệnh (hết quân)
    if (enemyOtherCount == 0 && playerOtherCount == 0) {
        m_matchEndInfo.result = MatchResult::Draw;
        m_matchEndInfo.reason = "Quan luc hai ben deu da bi tieu diet sach! Hoa tran.";
        return;
    }

    // 6. Hết giờ: bên nào có tổng quân gộp nhiều hơn thắng; bằng nhau thì hòa
    if (m_currentTick >= getMatchDurationTicks()) {
        if (playerTotalUnits > enemyTotalUnits) {
            m_matchEndInfo.result = MatchResult::Victory;
            m_matchEndInfo.reason = "Het thoi gian! Phe ban thang nho tong quan so ap dao (" +
                                    std::to_string(playerTotalUnits) + " vs " + std::to_string(enemyTotalUnits) + ").";
        } else if (enemyTotalUnits > playerTotalUnits) {
            m_matchEndInfo.result = MatchResult::Defeat;
            m_matchEndInfo.reason = "Het thoi gian! Phe dich thang nho tong quan so ap dao (" +
                                    std::to_string(enemyTotalUnits) + " vs " + std::to_string(playerTotalUnits) + ").";
        } else {
            m_matchEndInfo.result = MatchResult::Draw;
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

} // namespace sim
