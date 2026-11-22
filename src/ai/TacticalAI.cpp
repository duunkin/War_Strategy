#include "TacticalAI.hpp"
#include "../sim/combat/CombatSystem.hpp"
#include "../sim/pathfinding/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ai {

TacticalAI::TacticalAI(sim::Faction controlledFaction, uint64_t seed, const AiParams& params)
    : m_controlledFaction(controlledFaction), m_params(params), m_rng(seed) {
    m_lastSnapshot.faction = m_controlledFaction;
    m_lastSnapshot.params = m_params;
}

void TacticalAI::reset(uint64_t newSeed) {
    m_rng.setSeed(newSeed);
    m_lastDecisionTick = 0;
    m_isFirstDecision = true;
    m_memory.clear();
    m_lastSnapshot = AiThoughtSnapshot{};
    m_lastSnapshot.faction = m_controlledFaction;
    m_lastSnapshot.params = m_params;
    m_totalCommandsIssued = 0;
    m_unitIntents.clear();
}

void TacticalAI::update(sim::WorldSim& sim) {
    std::vector<sim::Command> cmds = plan(sim);
    for (const auto& cmd : cmds) {
        sim.queueCommand(cmd);
    }
}

void TacticalAI::updateEnemyMemory(const sim::WorldSim& sim) {
    uint64_t currentTick = sim.getCurrentTick();
    const auto& units = sim.getUnits();

    // 1. Cập nhật các đơn vị địch nhìn thấy được trong tick hiện tại
    for (const auto& u : units) {
        if (u.isDead()) continue;
        if (u.getFaction() == m_controlledFaction || u.getFaction() == sim::Faction::Neutral) continue;

        // TUÂN THỦ FOG OF WAR: Chỉ quan sát nếu hệ thống tầm nhìn xác nhận nhìn thấy
        if (!sim.isUnitVisible(u, m_controlledFaction)) continue;

        bool found = false;
        for (auto& record : m_memory) {
            if (record.unitId == u.getId()) {
                record.lastTileX = u.getTileX();
                record.lastTileY = u.getTileY();
                record.lastSeenTick = currentTick;
                record.lastHp = u.getHp();
                record.maxHp = u.getMaxHp();
                record.armor = u.getArmor();
                found = true;
                break;
            }
        }

        if (!found) {
            EnemyMemoryRecord rec;
            rec.unitId = u.getId();
            rec.faction = u.getFaction();
            rec.type = u.getType();
            rec.domain = u.getDomain();
            rec.lastTileX = u.getTileX();
            rec.lastTileY = u.getTileY();
            rec.lastSeenTick = currentTick;
            rec.lastHp = u.getHp();
            rec.maxHp = u.getMaxHp();
            rec.armor = u.getArmor();
            m_memory.push_back(rec);
        }
    }

    // 2. Dọn dẹp các bản ghi đã quá thời gian lưu giữ (targetMemoryTicks)
    // hoặc đơn vị đã chết và ô đó đang trong tầm nhìn thấy rõ
    m_memory.erase(
        std::remove_if(m_memory.begin(), m_memory.end(), [&](const EnemyMemoryRecord& r) {
            if (currentTick >= r.lastSeenTick + m_params.targetMemoryTicks) {
                return true;
            }
            // Nếu đơn vị trong sim đã chết hoặc biến mất
            const sim::Unit* u = sim.getUnitById(r.unitId);
            if (!u || u->isDead()) {
                return true;
            }
            return false;
        }),
        m_memory.end()
    );
}

std::vector<sim::Command> TacticalAI::plan(const sim::WorldSim& sim) {
    std::vector<sim::Command> commands;
    if (sim.isGameOver()) return commands;

    uint64_t currentTick = sim.getCurrentTick();
    updateEnemyMemory(sim);

    // Chu kỳ ra quyết định: chia đều các quân chủng theo các sub-phase tất định trong chu kỳ
    uint32_t interval = std::max(4u, m_params.decisionIntervalTicks);
    uint32_t subPhase = 0xFF; // Mặc định chạy toàn bộ ở tick đầu tiên
    bool shouldPlan = false;

    if (m_isFirstDecision) {
        m_isFirstDecision = false;
        m_lastDecisionTick = currentTick;
        shouldPlan = true;
        subPhase = 0xFF;
    } else {
        uint32_t offset = static_cast<uint32_t>((currentTick - m_lastDecisionTick) % interval);
        uint32_t p1 = interval / 4;
        uint32_t p2 = interval / 2;
        uint32_t p3 = (3 * interval) / 4;

        if (offset == 0) {
            shouldPlan = true;
            subPhase = 0; // Commander & Artillery/Gepard
        } else if (offset == p1) {
            shouldPlan = true;
            subPhase = 1; // Air & Navy
        } else if (offset == p2) {
            shouldPlan = true;
            subPhase = 2; // Fast ground (Puma, Heavy tanks)
        } else if (offset == p3) {
            shouldPlan = true;
            subPhase = 3; // Infantry
        }
    }

    // Kiểm tra khẩn cấp: nếu tư lệnh bị thương hoặc có địch áp sát, luôn cho tư lệnh thoát hiểm
    bool emergencyCmdr = false;
    if (!shouldPlan) {
        for (const auto& u : sim.getUnits()) {
            if (!u.isDead() && u.getFaction() == m_controlledFaction && u.getType() == sim::UnitType::Commander) {
                if (u.getHp() < u.getMaxHp()) {
                    emergencyCmdr = true;
                } else {
                    float safeR = m_params.commanderSafeRadiusTiles * m_params.commanderSafeRadiusTiles;
                    int cx = u.getTileX();
                    int cy = u.getTileY();
                    for (const auto& enemy : m_memory) {
                        int dx = enemy.lastTileX - cx;
                        int dy = enemy.lastTileY - cy;
                        if (static_cast<float>(dx * dx + dy * dy) <= safeR) {
                            emergencyCmdr = true;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    if (!shouldPlan && !emergencyCmdr) {
        return commands;
    }

    // Phân loại lực lượng phe mình
    const sim::Unit* commander = nullptr;
    std::vector<const sim::Unit*> infantry;
    std::vector<const sim::Unit*> pumas;
    std::vector<const sim::Unit*> heavyTanks; // Leopard 2A, T-14
    std::vector<const sim::Unit*> caesars;
    std::vector<const sim::Unit*> gepards;
    std::vector<const sim::Unit*> su57s;
    std::vector<const sim::Unit*> b2s;
    std::vector<const sim::Unit*> ships;
    std::vector<const sim::Unit*> friendlyUnits;

    for (const auto& u : sim.getUnits()) {
        if (u.isDead() || u.getFaction() != m_controlledFaction) continue;

        friendlyUnits.push_back(&u);
        switch (u.getType()) {
            case sim::UnitType::Commander:
                commander = &u;
                break;
            case sim::UnitType::Infantry:
                infantry.push_back(&u);
                break;
            case sim::UnitType::Puma:
                pumas.push_back(&u);
                break;
            case sim::UnitType::Leopard2A:
            case sim::UnitType::T14Armata:
                heavyTanks.push_back(&u);
                break;
            case sim::UnitType::Caesar:
                caesars.push_back(&u);
                break;
            case sim::UnitType::Gepard:
                gepards.push_back(&u);
                break;
            case sim::UnitType::Su57:
                su57s.push_back(&u);
                break;
            case sim::UnitType::B2Spirit:
                b2s.push_back(&u);
                break;
            case sim::UnitType::Mogami:
            case sim::UnitType::Asagiri:
                ships.push_back(&u);
                break;
            default:
                break;
        }
    }

    // 1. Lập kế hoạch cho Tư lệnh (Commander)
    size_t cmdBeforeCmdr = commands.size();
    if (commander && (subPhase == 0xFF || subPhase == 0 || emergencyCmdr)) {
        planCommander(sim, *commander, friendlyUnits, commands);
    }
    bool cmdFlee = (commands.size() > cmdBeforeCmdr);

    // 2. Lập kế hoạch cho Pháo binh (Caesar) & Phòng không (Gepard)
    size_t cmdBeforeArty = commands.size();
    std::vector<const sim::Unit*> mainForce = heavyTanks;
    mainForce.insert(mainForce.end(), infantry.begin(), infantry.end());
    if (subPhase == 0xFF || subPhase == 0) {
        planArtilleryAndAntiAir(sim, caesars, gepards, commander, mainForce, commands);
    }
    bool artyAction = (commands.size() > cmdBeforeArty);

    // 3. Lập kế hoạch cho Không quân (Su-57, B-2 Spirit)
    size_t cmdBeforeAir = commands.size();
    if (subPhase == 0xFF || subPhase == 1) {
        planAirForce(sim, su57s, b2s, commands);
    }
    bool airAction = (commands.size() > cmdBeforeAir);

    // 4. Lập kế hoạch cho Thủy quân (Mogami, Asagiri)
    size_t cmdBeforeNavy = commands.size();
    if (subPhase == 0xFF || subPhase == 1) {
        planNavy(sim, ships, commands);
    }
    bool navyAction = (commands.size() > cmdBeforeNavy);

    // 5. Lập kế hoạch cho Bộ binh & Tăng thiết giáp (Infantry, Puma, Leopard 2A, T-14)
    size_t cmdBeforeGround = commands.size();
    if (subPhase == 0xFF) {
        std::vector<const sim::Unit*> groundStrikeUnits;
        groundStrikeUnits.insert(groundStrikeUnits.end(), pumas.begin(), pumas.end());
        groundStrikeUnits.insert(groundStrikeUnits.end(), heavyTanks.begin(), heavyTanks.end());
        groundStrikeUnits.insert(groundStrikeUnits.end(), infantry.begin(), infantry.end());
        planInfantryAndArmor(sim, groundStrikeUnits, commander, commands);
    } else if (subPhase == 2) {
        std::vector<const sim::Unit*> fastStrikeUnits;
        fastStrikeUnits.insert(fastStrikeUnits.end(), pumas.begin(), pumas.end());
        fastStrikeUnits.insert(fastStrikeUnits.end(), heavyTanks.begin(), heavyTanks.end());
        planInfantryAndArmor(sim, fastStrikeUnits, commander, commands);
    } else if (subPhase == 3) {
        planInfantryAndArmor(sim, infantry, commander, commands);
    }
    bool groundAction = (commands.size() > cmdBeforeGround);

    // =========================================================================
    // Cập nhật Snapshot Telemetry (M8)
    // =========================================================================
    bool commanderInDanger = false;
    if (commander) {
        int comX = commander->getTileX();
        int comY = commander->getTileY();
        float safeRadiusSq = m_params.commanderSafeRadiusTiles * m_params.commanderSafeRadiusTiles;
        for (const auto& enemy : m_memory) {
            int dx = enemy.lastTileX - comX;
            int dy = enemy.lastTileY - comY;
            if (static_cast<float>(dx * dx + dy * dy) <= safeRadiusSq) {
                commanderInDanger = true;
                break;
            }
        }
        if (commander->getHp() < commander->getMaxHp()) {
            commanderInDanger = true;
        }
    }

    int visibleEnemies = 0;
    for (const auto& u : sim.getUnits()) {
        if (!u.isDead() && u.getFaction() != m_controlledFaction &&
            u.getFaction() != sim::Faction::Neutral && sim.isUnitVisible(u, m_controlledFaction)) {
            visibleEnemies++;
        }
    }

    int rememberedEnemies = static_cast<int>(m_memory.size());
    int aliveUnits = static_cast<int>(friendlyUnits.size());
    int totalTroops = 0;
    int retreatingUnits = 0;
    int scoutingUnits = 0;
    int attackingUnits = 0;

    for (const auto* u : friendlyUnits) {
        totalTroops += u->getCurrentUnits();
        float hpRatio = static_cast<float>(u->getHp()) / static_cast<float>(std::max(1, u->getMaxHp()));
        if (m_params.enableUnitRetreat && hpRatio < m_params.retreatHpRatio) {
            retreatingUnits++;
        } else if (u->getType() == sim::UnitType::Puma || (u->getType() == sim::UnitType::Su57 && m_memory.empty())) {
            scoutingUnits++;
        } else if (u->getType() == sim::UnitType::Commander) {
            if (commanderInDanger) retreatingUnits++;
            else scoutingUnits++;
        } else {
            attackingUnits++;
        }
    }

    AiPosture posture = AiPosture::Scouting;
    if (commanderInDanger) {
        posture = AiPosture::DefendCmd;
    } else if (retreatingUnits >= 4 || (aliveUnits > 0 && retreatingUnits > aliveUnits / 3)) {
        posture = AiPosture::Retreating;
    } else if (rememberedEnemies == 0) {
        posture = AiPosture::Scouting;
    } else if (m_params.aggression >= 0.8f && rememberedEnemies >= 4) {
        posture = AiPosture::AllOut;
    } else if (rememberedEnemies > 0) {
        posture = AiPosture::Engaging;
    } else {
        posture = AiPosture::Advancing;
    }

    std::vector<std::string> intents;
    if (cmdFlee) intents.push_back("CmdFlee");
    if (artyAction) intents.push_back("ArtyStrike");
    if (airAction) {
        if (!m_memory.empty()) intents.push_back("AirHunt");
        else intents.push_back("AirScout");
    }
    if (navyAction) intents.push_back("NavalSupport");
    if (groundAction) {
        if (m_memory.empty()) intents.push_back("ScoutFront");
        else if (m_params.aggression >= 0.7f) intents.push_back("FlankPush");
        else intents.push_back("HoldForest");
    }
    if (intents.empty()) {
        if (posture == AiPosture::Scouting) intents.push_back("ScoutFront");
        else if (posture == AiPosture::Engaging) intents.push_back("FocusFire");
        else intents.push_back("HoldLine");
    }

    std::string primaryIntentStr = intents[0];
    if (intents.size() > 1) {
        primaryIntentStr += "," + intents[1];
    }

    m_totalCommandsIssued += static_cast<uint32_t>(commands.size());

    m_lastSnapshot.faction = m_controlledFaction;
    m_lastSnapshot.decisionTick = currentTick;
    m_lastSnapshot.posture = posture;
    m_lastSnapshot.primaryIntent = primaryIntentStr;
    m_lastSnapshot.visibleEnemies = visibleEnemies;
    m_lastSnapshot.rememberedEnemies = rememberedEnemies;
    m_lastSnapshot.aliveUnits = aliveUnits;
    m_lastSnapshot.totalTroops = totalTroops;
    m_lastSnapshot.commanderHp = commander ? commander->getHp() : 0;
    m_lastSnapshot.commanderMaxHp = commander ? commander->getMaxHp() : 100;
    m_lastSnapshot.commanderInDanger = commanderInDanger;
    m_lastSnapshot.attackingUnits = attackingUnits;
    m_lastSnapshot.scoutingUnits = scoutingUnits;
    m_lastSnapshot.retreatingUnits = retreatingUnits;
    m_lastSnapshot.commandsIssuedThisCycle = static_cast<uint32_t>(commands.size());
    m_lastSnapshot.totalCommandsIssued = m_totalCommandsIssued;
    m_lastSnapshot.params = m_params;

    return commands;
}

void TacticalAI::planCommander(const sim::WorldSim& sim, const sim::Unit& commander,
                              const std::vector<const sim::Unit*>& friendlyUnits,
                              std::vector<sim::Command>& outCommands) {
    int comX = commander.getTileX();
    int comY = commander.getTileY();

    // Tìm các mối nguy hiểm gần nhất trong bộ nhớ địch
    float safeRadiusSq = m_params.commanderSafeRadiusTiles * m_params.commanderSafeRadiusTiles;
    int threatCount = 0;
    int threatSumX = 0;
    int threatSumY = 0;

    for (const auto& enemy : m_memory) {
        int dx = enemy.lastTileX - comX;
        int dy = enemy.lastTileY - comY;
        float distSq = static_cast<float>(dx * dx + dy * dy);
        if (distSq <= safeRadiusSq) {
            threatCount++;
            threatSumX += enemy.lastTileX;
            threatSumY += enemy.lastTileY;
        }
    }

    bool isSeverelyDamaged = (commander.getHp() < commander.getMaxHp() / 2);

    // Nếu có địch áp sát trong bán kính nguy hiểm hoặc mất máu nặng -> Tháo chạy ngay lập tức!
    if (m_params.enableCommanderRetreat && (threatCount > 0 || isSeverelyDamaged)) {
        m_unitIntents[commander.getId()] = "Retreating (Danger)";
        int threatCenterX = (threatCount > 0) ? (threatSumX / threatCount) : (comX + (m_controlledFaction == sim::Faction::Enemy ? -5 : 5));
        int threatCenterY = (threatCount > 0) ? (threatSumY / threatCount) : comY;

        sim::TileCoord safeTile = findSafeRetreatTile(
            sim, comX, comY, threatCenterX, threatCenterY,
            sim::UnitDomain::Land, sim::UnitType::Commander, 4, 10
        );

        if (safeTile.x != comX || safeTile.y != comY) {
            sim::Command cmd;
            cmd.type = sim::CommandType::Move;
            cmd.move.unitIds = {commander.getId()};
            cmd.move.targetTileX = safeTile.x;
            cmd.move.targetTileY = safeTile.y;
            outCommands.push_back(cmd);
            return;
        }
    }

    // Nếu đang an toàn và được phép rút lui: nếu chưa có đường đi, có thể lùi nhẹ về cụm quân bảo vệ
    if (m_params.enableCommanderRetreat && !commander.hasPath() && !friendlyUnits.empty()) {
        // Giữ vị trí an toàn ở phía sau đội hình
        int homeBaseX = (m_controlledFaction == sim::Faction::Enemy) ? (sim.getMap().getWidth() - 12) : 12;
        int homeBaseY = sim.getMap().getHeight() / 2;
        int dxHome = homeBaseX - comX;
        int dyHome = homeBaseY - comY;
        float maxAllowedDist = m_params.commanderSafeRadiusTiles;
        if (static_cast<float>(dxHome * dxHome + dyHome * dyHome) > maxAllowedDist * maxAllowedDist) {
            m_unitIntents[commander.getId()] = "Retreating (Reposition)";
            sim::TileCoord retreatTarget = findSafeRetreatTile(sim, comX, comY, comX + (m_controlledFaction == sim::Faction::Enemy ? -5 : 5), comY, sim::UnitDomain::Land, sim::UnitType::Commander, 3, 6);
            if (retreatTarget.x != comX || retreatTarget.y != comY) {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {commander.getId()};
                cmd.move.targetTileX = retreatTarget.x;
                cmd.move.targetTileY = retreatTarget.y;
                outCommands.push_back(cmd);
            }
        } else {
            m_unitIntents[commander.getId()] = "Standing (At Base)";
        }
    } else {
        m_unitIntents[commander.getId()] = "Standing (Safe)";
    }
}

void TacticalAI::planArtilleryAndAntiAir(const sim::WorldSim& sim,
                                        const std::vector<const sim::Unit*>& caesars,
                                        const std::vector<const sim::Unit*>& gepards,
                                        const sim::Unit* commander,
                                        const std::vector<const sim::Unit*>& mainForce,
                                        std::vector<sim::Command>& outCommands) {
    // 1. Xử lý Pháo binh (Caesar): duy trì stand-off distance
    for (const auto* caesar : caesars) {
        if (!caesar || caesar->isDead()) continue;

        int cx = caesar->getTileX();
        int cy = caesar->getTileY();
        float range = caesar->getRangeTiles();

        // Tìm mục tiêu địch trong bộ nhớ
        const EnemyMemoryRecord* bestTarget = nullptr;
        float bestScore = -1.0f;

        for (const auto& enemy : m_memory) {
            if (enemy.domain == sim::UnitDomain::Air) continue; // Pháo không bắn máy bay

            int dx = enemy.lastTileX - cx;
            int dy = enemy.lastTileY - cy;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

            // Điểm giá trị mục tiêu: ưu tiên bộ binh đông, pháo/tăng
            float value = 1.0f;
            if (enemy.type == sim::UnitType::Infantry) value = 3.0f;
            else if (enemy.type == sim::UnitType::Caesar) value = 2.5f;
            else if (enemy.type == sim::UnitType::Leopard2A || enemy.type == sim::UnitType::T14Armata) value = 2.0f;
            else if (enemy.type == sim::UnitType::Commander) value = 5.0f;

            float score = value / (1.0f + dist * 0.1f);
            if (score > bestScore) {
                bestScore = score;
                bestTarget = &enemy;
            }
        }

        if (bestTarget) {
            int dx = bestTarget->lastTileX - cx;
            int dy = bestTarget->lastTileY - cy;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

            // Nếu địch áp sát quá gần: Lùi lại gấp (ngưỡng điều khiển bởi artilleryStandOffMargin)
            float retreatThreshold = std::max(2.0f, range * m_params.artilleryIdealRangeRatio - m_params.artilleryStandOffMargin);
            if (dist < retreatThreshold) {
                m_unitIntents[caesar->getId()] = "Retreating (StandOff Margin)";
                sim::TileCoord retreatTile = findSafeRetreatTile(
                    sim, cx, cy, bestTarget->lastTileX, bestTarget->lastTileY,
                    sim::UnitDomain::Land, sim::UnitType::Caesar, 3, 6
                );
                if (retreatTile.x != cx || retreatTile.y != cy) {
                    sim::Command cmd;
                    cmd.type = sim::CommandType::Move;
                    cmd.move.unitIds = {caesar->getId()};
                    cmd.move.targetTileX = retreatTile.x;
                    cmd.move.targetTileY = retreatTile.y;
                    outCommands.push_back(cmd);
                }
            }
            // Nếu ngoài tầm bắn: Tiến đến ô stand-off lý tưởng (khoảng 80% range)
            else if (dist > range) {
                m_unitIntents[caesar->getId()] = "Advancing (To Target)";
                sim::TileCoord standOffTile = findArtilleryStandOffTile(
                    sim, *caesar, bestTarget->lastTileX, bestTarget->lastTileY,
                    range * m_params.artilleryIdealRangeRatio
                );
                if (standOffTile.x != cx || standOffTile.y != cy) {
                    if (caesar->hasPath() && caesar->getFinalTargetTile() == standOffTile) {
                        // Đang đi tới ô stand-off lý tưởng này
                    } else {
                        sim::Command cmd;
                        cmd.type = sim::CommandType::Move;
                        cmd.move.unitIds = {caesar->getId()};
                        cmd.move.targetTileX = standOffTile.x;
                        cmd.move.targetTileY = standOffTile.y;
                        outCommands.push_back(cmd);
                    }
                }
            }
            else {
                m_unitIntents[caesar->getId()] = "Attacking (In Range)";
            }
            // Nếu đã nằm trong khoảng bắn [5.0f, range]: Giữ nguyên vị trí để tự động bắn!
        } else if (!caesar->hasPath() && !mainForce.empty()) {
            m_unitIntents[caesar->getId()] = "Advancing (Follow Main Force)";
            // Chưa thấy địch: di chuyển theo sau cụm quân chủ lực (cách khoảng 6 ô)
            const auto* leader = mainForce[0];
            int lx = leader->getTileX();
            int ly = leader->getTileY();
            sim::TileCoord tgt = findSafeRetreatTile(sim, cx, cy, lx, ly, sim::UnitDomain::Land, sim::UnitType::Caesar, 2, 4);
            sim::Command cmd;
            cmd.type = sim::CommandType::Move;
            cmd.move.unitIds = {caesar->getId()};
            cmd.move.targetTileX = tgt.x;
            cmd.move.targetTileY = tgt.y;
            outCommands.push_back(cmd);
        } else {
            m_unitIntents[caesar->getId()] = "Standing (No Targets)";
        }
    }

    // 2. Xử lý Phòng không (Gepard): Ưu tiên đánh chặn máy bay địch hoặc hộ tống
    const EnemyMemoryRecord* airThreat = nullptr;
    for (const auto& enemy : m_memory) {
        if (enemy.domain == sim::UnitDomain::Air) {
            airThreat = &enemy;
            break;
        }
    }

    for (size_t i = 0; i < gepards.size(); ++i) {
        const auto* gepard = gepards[i];
        if (!gepard || gepard->isDead()) continue;

        int gx = gepard->getTileX();
        int gy = gepard->getTileY();

        // Ưu tiên số 1: Đánh chặn máy bay địch đã phát hiện
        if (airThreat) {
            m_unitIntents[gepard->getId()] = "Attacking (Intercept Air)";
            sim::Command cmd;
            cmd.type = sim::CommandType::Move;
            cmd.move.unitIds = {gepard->getId()};
            cmd.move.targetTileX = airThreat->lastTileX;
            cmd.move.targetTileY = airThreat->lastTileY;
            outCommands.push_back(cmd);
        }
        // Ưu tiên số 2: Hộ tống Caesar hoặc Commander hoặc chủ lực
        else if (!gepard->hasPath()) {
            int escortTargetX = gx;
            int escortTargetY = gy;

            if (i == 0 && commander) {
                m_unitIntents[gepard->getId()] = "Defending (Escort Cmdr)";
                // Gepard 1 bảo vệ Tư lệnh
                escortTargetX = commander->getTileX() + (m_controlledFaction == sim::Faction::Enemy ? -2 : 2);
                escortTargetY = commander->getTileY();
            } else if (!caesars.empty()) {
                m_unitIntents[gepard->getId()] = "Defending (Escort Caesar)";
                // Gepard 2 bảo vệ Caesar
                escortTargetX = caesars[0]->getTileX() + (m_controlledFaction == sim::Faction::Enemy ? -2 : 2);
                escortTargetY = caesars[0]->getTileY();
            } else if (!mainForce.empty()) {
                m_unitIntents[gepard->getId()] = "Defending (Escort Main Force)";
                escortTargetX = mainForce[0]->getTileX();
                escortTargetY = mainForce[0]->getTileY();
            } else {
                m_unitIntents[gepard->getId()] = "Standing";
            }

            escortTargetX = std::clamp(escortTargetX, 2, sim.getMap().getWidth() - 3);
            escortTargetY = std::clamp(escortTargetY, 2, sim.getMap().getHeight() - 3);

            sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(sim.getMap(), escortTargetX, escortTargetY);
            if (sim.getConfig().isPassable(dt, sim::UnitDomain::Land, sim::UnitType::Gepard)) {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {gepard->getId()};
                cmd.move.targetTileX = escortTargetX;
                cmd.move.targetTileY = escortTargetY;
                outCommands.push_back(cmd);
            }
        }
    }
}

void TacticalAI::planAirForce(const sim::WorldSim& sim,
                             const std::vector<const sim::Unit*>& su57s,
                             const std::vector<const sim::Unit*>& b2s,
                             std::vector<sim::Command>& outCommands) {
    // 1. Tiêm kích Su-57: Săn máy bay địch hoặc trinh sát mở sương mù / hỗ trợ hỏa lực
    const EnemyMemoryRecord* enemyAir = nullptr;
    for (const auto& enemy : m_memory) {
        if (enemy.domain == sim::UnitDomain::Air) {
            enemyAir = &enemy;
            break;
        }
    }

    for (size_t i = 0; i < su57s.size(); ++i) {
        const auto* su = su57s[i];
        if (!su || su->isDead()) continue;

        if (enemyAir) {
            m_unitIntents[su->getId()] = "Attacking (Air Intercept)";
            // Săn máy bay địch
            if (su->hasPath() && su->getFinalTargetTile().x == enemyAir->lastTileX && su->getFinalTargetTile().y == enemyAir->lastTileY) {
                // Đang bay bám đuổi máy bay địch
            } else {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {su->getId()};
                cmd.move.targetTileX = enemyAir->lastTileX;
                cmd.move.targetTileY = enemyAir->lastTileY;
                outCommands.push_back(cmd);
            }
        } else if (!m_memory.empty()) {
            m_unitIntents[su->getId()] = "Attacking (Ground Support)";
            // Đánh hỗ trợ mục tiêu mặt đất
            const auto& enemyGround = m_memory[i % m_memory.size()];
            if (su->hasPath() && su->getFinalTargetTile().x == enemyGround.lastTileX && su->getFinalTargetTile().y == enemyGround.lastTileY) {
                // Đang bay tới mục tiêu
            } else {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {su->getId()};
                cmd.move.targetTileX = enemyGround.lastTileX;
                cmd.move.targetTileY = enemyGround.lastTileY;
                outCommands.push_back(cmd);
            }
        } else {
            m_unitIntents[su->getId()] = "Scouting (Air Recon)";
            if (!su->hasPath()) {
                // Tuần tra trinh sát sâu sang nửa bản đồ đối phương
                sim::TileCoord scoutTarget = pickScoutingTarget(sim, static_cast<int>(i), static_cast<int>(su57s.size()), sim::UnitDomain::Air, sim::UnitType::Su57);
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {su->getId()};
                cmd.move.targetTileX = scoutTarget.x;
                cmd.move.targetTileY = scoutTarget.y;
                outCommands.push_back(cmd);
            }
        }
    }

    // 2. Oanh tạc cơ B-2 Spirit: Ưu tiên tấn công mục tiêu giá trị cao và tránh cụm Gepard
    const EnemyMemoryRecord* enemyGepard = nullptr;
    for (const auto& enemy : m_memory) {
        if (enemy.type == sim::UnitType::Gepard) {
            enemyGepard = &enemy;
            break;
        }
    }

    const EnemyMemoryRecord* highValueTarget = findHighValueEnemyTarget(sim::UnitDomain::Land);

    for (const auto* b2 : b2s) {
        if (!b2 || b2->isDead()) continue;

        if (highValueTarget) {
            m_unitIntents[b2->getId()] = "Attacking (Bombing)";
            int targetX = highValueTarget->lastTileX;
            int targetY = highValueTarget->lastTileY;

            // Nếu mục tiêu quá gần Gepard địch đã phát hiện, lùi né nhẹ để không bay thẳng vào ổ hỏa lực
            if (enemyGepard) {
                int gdx = enemyGepard->lastTileX - targetX;
                int gdy = enemyGepard->lastTileY - targetY;
                if (gdx * gdx + gdy * gdy < 4 * 4) {
                    targetY += (targetY > sim.getMap().getHeight() / 2) ? -4 : 4;
                    targetY = std::clamp(targetY, 2, sim.getMap().getHeight() - 3);
                }
            }

            if (b2->hasPath() && b2->getFinalTargetTile().x == targetX && b2->getFinalTargetTile().y == targetY) {
                // Đang bay tới
            } else {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {b2->getId()};
                cmd.move.targetTileX = targetX;
                cmd.move.targetTileY = targetY;
                outCommands.push_back(cmd);
            }
        } else {
            m_unitIntents[b2->getId()] = "Scouting (Stealth Recon)";
            if (!b2->hasPath()) {
                // Tuần tra trinh sát các điểm chiến lược
                sim::TileCoord scoutTarget = pickScoutingTarget(sim, 1, 2, sim::UnitDomain::Air, sim::UnitType::B2Spirit);
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {b2->getId()};
                cmd.move.targetTileX = scoutTarget.x;
                cmd.move.targetTileY = scoutTarget.y;
                outCommands.push_back(cmd);
            }
        }
    }
}

void TacticalAI::planNavy(const sim::WorldSim& sim,
                          const std::vector<const sim::Unit*>& ships,
                          std::vector<sim::Command>& outCommands) {
    if (ships.empty()) return;

    // Tìm mục tiêu ưu tiên cho tàu chiến:
    // 1. Tàu địch (Mogami, Asagiri)
    // 2. Mục tiêu lục quân địch nằm gần bờ biển (trong khoảng cách có thể bắn tới)
    const EnemyMemoryRecord* shipTarget = nullptr;
    const EnemyMemoryRecord* coastalTarget = nullptr;

    for (const auto& enemy : m_memory) {
        if (enemy.domain == sim::UnitDomain::Sea) {
            shipTarget = &enemy;
            break;
        } else if (enemy.domain == sim::UnitDomain::Land && !coastalTarget) {
            // Kiểm tra xem mục tiêu có gần biển không (bán kính <= 10 ô)
            coastalTarget = &enemy;
        }
    }

    const auto& map = sim.getMap();
    int mapW = map.getWidth();
    int mapH = map.getHeight();

    for (size_t i = 0; i < ships.size(); ++i) {
        const auto* ship = ships[i];
        if (!ship || ship->isDead()) continue;

        int sx = ship->getTileX();
        int sy = ship->getTileY();

        if (shipTarget) {
            m_unitIntents[ship->getId()] = "Attacking (Enemy Ship)";
            // Tìm ô biển gần tàu địch nhất để tiếp chiến
            sim::TileCoord bestSeaTile{sx, sy};
            int bestDistSq = std::numeric_limits<int>::max();

            for (int r = 1; r <= 8; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue;
                        int nx = shipTarget->lastTileX + dx;
                        int ny = shipTarget->lastTileY + dy;
                        if (!map.isInBounds(nx, ny)) continue;

                        sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                        if (sim.getConfig().isPassable(dt, sim::UnitDomain::Sea, ship->getType())) {
                            int ddx = nx - sx;
                            int ddy = ny - sy;
                            int dSq = ddx * ddx + ddy * ddy;
                            if (dSq < bestDistSq) {
                                bestDistSq = dSq;
                                bestSeaTile = {nx, ny};
                            }
                        }
                    }
                }
                if (bestDistSq < std::numeric_limits<int>::max()) break;
            }

            if (bestSeaTile.x != sx || bestSeaTile.y != sy) {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {ship->getId()};
                cmd.move.targetTileX = bestSeaTile.x;
                cmd.move.targetTileY = bestSeaTile.y;
                outCommands.push_back(cmd);
            }
        } else if (coastalTarget) {
            m_unitIntents[ship->getId()] = "Attacking (Coastal Target)";
            // Tìm ô biển trong tầm bắn (range <= 10) tới mục tiêu lục quân ven bờ
            sim::TileCoord bestCoastalSeaTile{sx, sy};
            bool found = false;

            for (int r = 2; r <= 10; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue;
                        int nx = coastalTarget->lastTileX + dx;
                        int ny = coastalTarget->lastTileY + dy;
                        if (!map.isInBounds(nx, ny)) continue;

                        sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                        if (sim.getConfig().isPassable(dt, sim::UnitDomain::Sea, ship->getType())) {
                            bestCoastalSeaTile = {nx, ny};
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (found) break;
            }

            if (found && (bestCoastalSeaTile.x != sx || bestCoastalSeaTile.y != sy)) {
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {ship->getId()};
                cmd.move.targetTileX = bestCoastalSeaTile.x;
                cmd.move.targetTileY = bestCoastalSeaTile.y;
                outCommands.push_back(cmd);
            }
        } else {
            m_unitIntents[ship->getId()] = "Scouting (Naval Patrol)";
            if (!ship->hasPath()) {
                // Tuần tra ven biển mở sương mù
                // Đi dọc theo bờ biển (xem các ô COASTAL_SEA phía địch)
                int patrolX = (m_controlledFaction == sim::Faction::Enemy) ? (mapW / 2 - 10) : (mapW / 2 + 10);
                int patrolY = (i % 2 == 0) ? (mapH / 4) : (3 * mapH / 4);

                for (int radius = 0; radius < 25; ++radius) {
                    bool found = false;
                    for (int dy = -radius; dy <= radius; ++dy) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            int nx = patrolX + dx;
                            int ny = patrolY + dy;
                            if (!map.isInBounds(nx, ny)) continue;
                            sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                            if (sim.getConfig().isPassable(dt, sim::UnitDomain::Sea, ship->getType())) {
                                sim::Command cmd;
                                cmd.type = sim::CommandType::Move;
                                cmd.move.unitIds = {ship->getId()};
                                cmd.move.targetTileX = nx;
                                cmd.move.targetTileY = ny;
                                outCommands.push_back(cmd);
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                    }
                    if (found) break;
                }
            }
        }
    }
}

void TacticalAI::planInfantryAndArmor(const sim::WorldSim& sim,
                                      const std::vector<const sim::Unit*>& strikeUnits,
                                      const sim::Unit* commander,
                                      std::vector<sim::Command>& outCommands) {
    if (strikeUnits.empty()) return;

    const auto& map = sim.getMap();

    // 1. Kiểm tra rút lui cho các đơn vị bị thương nặng
    std::vector<const sim::Unit*> readyUnits;
    for (const auto* u : strikeUnits) {
        if (!u || u->isDead()) continue;

        float hpRatio = static_cast<float>(u->getHp()) / static_cast<float>(std::max(1, u->getMaxHp()));
        if (m_params.enableUnitRetreat && hpRatio < m_params.retreatHpRatio) {
            // Rút lui về phía sau
            int ux = u->getTileX();
            int uy = u->getTileY();
            int threatDirX = (m_controlledFaction == sim::Faction::Enemy) ? -5 : 5;
            sim::TileCoord retreatTile = findSafeRetreatTile(sim, ux, uy, ux + threatDirX, uy, u->getDomain(), u->getType(), 3, 6);
            if (retreatTile.x != ux || retreatTile.y != uy) {
                m_unitIntents[u->getId()] = "Retreating (Low HP)";
                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {u->getId()};
                cmd.move.targetTileX = retreatTile.x;
                cmd.move.targetTileY = retreatTile.y;
                outCommands.push_back(cmd);
                continue;
            } else {
                m_unitIntents[u->getId()] = "Retreating (Edge/Trapped)";
            }
        }
        readyUnits.push_back(u);
    }

    if (readyUnits.empty()) return;

    // 2. Nếu đã phát hiện mục tiêu địch: Điều phối tấn công mục tiêu ưu tiên
    if (!m_memory.empty()) {
        std::unordered_map<uint32_t, int> targetCounts;

        for (const auto* u : readyUnits) {
            int ux = u->getTileX();
            int uy = u->getTileY();

            const EnemyMemoryRecord* bestEnemy = nullptr;
            float bestScore = -1.0f;

            for (const auto& enemy : m_memory) {
                // Kiểm tra vũ khí của đơn vị có thể bắn mục tiêu này không
                bool canHit = false;
                if (enemy.domain == sim::UnitDomain::Land && u->canTargetGround()) canHit = true;
                if (enemy.domain == sim::UnitDomain::Air && u->canTargetAir()) canHit = true;
                if (enemy.domain == sim::UnitDomain::Sea && u->canTargetSea()) canHit = true;
                if (!canHit && enemy.domain == sim::UnitDomain::Air) continue; // Bỏ qua máy bay nếu không có vũ khí phòng không

                int dx = enemy.lastTileX - ux;
                int dy = enemy.lastTileY - uy;
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

                // Tính điểm ưu tiên theo từng loại quân
                float priority = 1.0f;
                if (u->getType() == sim::UnitType::Leopard2A || u->getType() == sim::UnitType::T14Armata) {
                    // Tăng ưu tiên bắn tăng địch, pháo địch
                    if (enemy.type == sim::UnitType::Leopard2A || enemy.type == sim::UnitType::T14Armata) priority = 3.5f;
                    else if (enemy.type == sim::UnitType::Caesar) priority = 3.0f;
                    else if (enemy.type == sim::UnitType::Puma) priority = 2.0f;
                    else if (enemy.type == sim::UnitType::Commander) priority = 5.0f;
                } else if (u->getType() == sim::UnitType::Infantry) {
                    // Bộ binh ưu tiên mục tiêu gần và bộ binh/thiết giáp địch
                    if (enemy.type == sim::UnitType::Infantry) priority = 2.5f;
                    else if (enemy.type == sim::UnitType::Commander) priority = 4.0f;
                } else if (u->getType() == sim::UnitType::Puma) {
                    // Puma ưu tiên truy quét pháo và mục tiêu mỏng
                    if (enemy.type == sim::UnitType::Caesar) priority = 4.0f;
                    else if (enemy.type == sim::UnitType::Puma) priority = 2.5f;
                    else if (enemy.type == sim::UnitType::Commander) priority = 5.0f;
                }

                // Bảo vệ Tư lệnh: Nếu địch xâm nhập vùng an toàn của Tư lệnh (theo commanderSafeRadiusTiles), tăng mạnh ưu tiên đánh chặn
                if (commander) {
                    int cdx = enemy.lastTileX - commander->getTileX();
                    int cdy = enemy.lastTileY - commander->getTileY();
                    float distCmdSq = static_cast<float>(cdx * cdx + cdy * cdy);
                    float safeRadiusSq = m_params.commanderSafeRadiusTiles * m_params.commanderSafeRadiusTiles;
                    if (distCmdSq <= safeRadiusSq) {
                        priority += 5.0f;
                    }
                }

                // Dồn hỏa lực (focusFireWeight): nhân vào điểm ưu tiên của mục tiêu đang bị nhiều đơn vị ta nhắm; nếu 1.0 thì giữ hành vi cũ
                auto itCount = targetCounts.find(enemy.unitId);
                if (itCount != targetCounts.end() && itCount->second > 0) {
                    priority *= std::pow(m_params.focusFireWeight, static_cast<float>(itCount->second));
                }

                // Cân nhắc khoảng cách: aggression cao sẵn sàng truy kích mục tiêu xa hơn
                float distPenalty = 0.15f / std::clamp(m_params.aggression, 0.2f, 2.0f);
                float score = priority / (1.0f + dist * distPenalty);
                if (score > bestScore) {
                    bestScore = score;
                    bestEnemy = &enemy;
                }
            }

            if (bestEnemy) {
                m_unitIntents[u->getId()] = "Attacking (Target " + std::to_string(bestEnemy->unitId) + ")";
                targetCounts[bestEnemy->unitId]++;
                int targetX = bestEnemy->lastTileX;
                int targetY = bestEnemy->lastTileY;

                // Với bộ binh: tìm ô địa hình có lợi (Rừng/Đồi) gần mục tiêu trong bán kính 2-3 ô để tận dụng cover
                if (u->getType() == sim::UnitType::Infantry) {
                    for (int r = 1; r <= 3; ++r) {
                        bool foundCover = false;
                        for (int dy = -r; dy <= r; ++dy) {
                            for (int dx = -r; dx <= r; ++dx) {
                                int cx = targetX + dx;
                                int cy = targetY + dy;
                                if (!map.isInBounds(cx, cy)) continue;

                                sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, cx, cy);
                                if (dt == sim::DetailedTerrainType::Forest || dt == sim::DetailedTerrainType::Hill) {
                                    if (sim.getConfig().isPassable(dt, u->getDomain(), u->getType())) {
                                        targetX = cx;
                                        targetY = cy;
                                        foundCover = true;
                                        break;
                                    }
                                }
                            }
                            if (foundCover) break;
                        }
                        if (foundCover) break;
                    }
                }

                if (u->hasPath() && u->getFinalTargetTile().x == targetX && u->getFinalTargetTile().y == targetY) {
                    continue; // Đang đi mượt trên đường tới đúng mục tiêu
                }

                sim::Command cmd;
                cmd.type = sim::CommandType::Move;
                cmd.move.unitIds = {u->getId()};
                cmd.move.targetTileX = targetX;
                cmd.move.targetTileY = targetY;
                outCommands.push_back(cmd);
            } else {
                m_unitIntents[u->getId()] = "Holding (No Valid Target)";
            }
        }
    }
    // 3. Khi CHƯA thấy địch: Chia lực lượng trinh sát & tiến công đa hướng
    else {
        // Nhóm Puma: trinh sát tiền tiêu (scoutingWeight & scoutingGroupCount điều khiển số lượng và độ vươn xa)
        int maxScouts = std::clamp(static_cast<int>(std::round(m_params.scoutingGroupCount * std::clamp(m_params.scoutingWeight, 0.4f, 2.0f))), 1, 8);
        int scoutCount = 0;
        std::vector<const sim::Unit*> combatUnits;

        for (const auto* u : readyUnits) {
            if (u->getType() == sim::UnitType::Puma && scoutCount < maxScouts) {
                m_unitIntents[u->getId()] = "Scouting (Puma Recon)";
                if (!u->hasPath()) {
                    sim::TileCoord scoutTarget = pickScoutingTarget(
                        sim, scoutCount++, maxScouts,
                        u->getDomain(), u->getType()
                    );
                    sim::Command cmd;
                    cmd.type = sim::CommandType::Move;
                    cmd.move.unitIds = {u->getId()};
                    cmd.move.targetTileX = scoutTarget.x;
                    cmd.move.targetTileY = scoutTarget.y;
                    outCommands.push_back(cmd);
                }
            } else {
                combatUnits.push_back(u);
            }
        }

        // Nhóm Tăng chủ lực & Bộ binh: aggression điều khiển tỉ lệ tấn công vs giữ hậu phương và cự ly tiến quân
        int halfW = map.getWidth() / 2;
        int advanceOffset = static_cast<int>(6.0f + 24.0f * std::clamp(m_params.aggression, 0.15f, 1.50f));
        int offensiveDestX = (m_controlledFaction == sim::Faction::Enemy) ? (halfW - advanceOffset) : (halfW + advanceOffset);
        offensiveDestX = std::clamp(offensiveDestX, 4, map.getWidth() - 5);

        // Tỉ lệ quân tham gia tiến công (thay vì ở lại bảo vệ hậu tuyến)
        float attackRatio = std::clamp(0.25f + 0.75f * (m_params.aggression / 1.5f), 0.25f, 1.0f);
        int maxAttackers = std::max(1, static_cast<int>(std::round(combatUnits.size() * attackRatio)));

        int cmdX = (commander) ? commander->getTileX() : ((m_controlledFaction == sim::Faction::Enemy) ? (map.getWidth() - 12) : 12);
        int guardOffset = std::clamp(static_cast<int>(m_params.commanderSafeRadiusTiles * 0.45f), 2, 12);
        int rearX = (m_controlledFaction == sim::Faction::Enemy) ? (cmdX - guardOffset) : (cmdX + guardOffset);
        rearX = std::clamp(rearX, 2, map.getWidth() - 3);

        int combatIndex = 0;
        for (const auto* u : combatUnits) {
            bool isAttacking = (combatIndex < maxAttackers);
            combatIndex++;
            m_unitIntents[u->getId()] = isAttacking ? "Advancing (Offensive)" : "Holding (Rear Guard)";

            if (u->hasPath()) continue;

            int sector = (combatIndex % 3); // 0: Bắc, 1: Trung tâm, 2: Nam

            int destX = isAttacking ? offensiveDestX : rearX;
            int destY = (sector == 0) ? (map.getHeight() / 4)
                      : (sector == 1) ? (map.getHeight() / 2)
                                      : (3 * map.getHeight() / 4);

            // Kiểm tra và tìm ô passable gần destX, destY
            for (int r = 0; r < 15; ++r) {
                bool found = false;
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        int nx = destX + dx;
                        int ny = destY + dy;
                        if (!map.isInBounds(nx, ny)) continue;
                        sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                        if (sim.getConfig().isPassable(dt, u->getDomain(), u->getType())) {
                            sim::Command cmd;
                            cmd.type = sim::CommandType::Move;
                            cmd.move.unitIds = {u->getId()};
                            cmd.move.targetTileX = nx;
                            cmd.move.targetTileY = ny;
                            outCommands.push_back(cmd);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (found) break;
            }
        }
    }
}

sim::TileCoord TacticalAI::findSafeRetreatTile(const sim::WorldSim& sim,
                                              int currentTileX, int currentTileY,
                                              int threatCenterX, int threatCenterY,
                                              sim::UnitDomain domain, sim::UnitType type,
                                              int minRadius, int maxRadius) {
    const auto& map = sim.getMap();
    sim::TileCoord bestTile{currentTileX, currentTileY};
    float bestScore = -10000.0f;

    // Vector hướng né hiểm họa
    int dirX = currentTileX - threatCenterX;
    int dirY = currentTileY - threatCenterY;
    if (dirX == 0 && dirY == 0) {
        dirX = (m_controlledFaction == sim::Faction::Enemy) ? 1 : -1;
    }

    for (int r = minRadius; r <= maxRadius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                int nx = currentTileX + dx;
                int ny = currentTileY + dy;
                if (!map.isInBounds(nx, ny)) continue;

                sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                if (!sim.getConfig().isPassable(dt, domain, type)) continue;

                // Đánh giá điểm ô an toàn
                int dThreatX = nx - threatCenterX;
                int dThreatY = ny - threatCenterY;
                float distThreat = std::sqrt(static_cast<float>(dThreatX * dThreatX + dThreatY * dThreatY));

                // Hướng hợp với vector né
                float dotProduct = static_cast<float>(dx * dirX + dy * dirY);

                float score = distThreat * 15.0f + dotProduct * 2.0f;

                // Thưởng điểm theo địa hình phòng thủ
                if (dt == sim::DetailedTerrainType::Forest) {
                    score += 35.0f; // Rừng che chắn tầm nhìn và phục kích
                } else if (dt == sim::DetailedTerrainType::Hill) {
                    score += 25.0f; // Đồi tăng tầm nhìn và phòng thủ
                } else if (dt == sim::DetailedTerrainType::Mountain) {
                    score += 10.0f;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestTile = {nx, ny};
                }
            }
        }
    }

    return bestTile;
}

sim::TileCoord TacticalAI::findArtilleryStandOffTile(const sim::WorldSim& sim,
                                                    const sim::Unit& artillery,
                                                    int targetTileX, int targetTileY,
                                                    float idealRangeTiles) {
    const auto& map = sim.getMap();
    int ax = artillery.getTileX();
    int ay = artillery.getTileY();

    sim::TileCoord bestTile{ax, ay};
    float bestDiff = 999.0f;

    int rTarget = static_cast<int>(std::round(idealRangeTiles));
    for (int r = std::max(2, rTarget - 2); r <= rTarget + 2; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;

                int nx = targetTileX + dx;
                int ny = targetTileY + dy;
                if (!map.isInBounds(nx, ny)) continue;

                sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                if (!sim.getConfig().isPassable(dt, artillery.getDomain(), artillery.getType())) continue;

                // Chọn ô gần vị trí hiện tại của pháo nhất và gần idealRange nhất
                float distToTarget = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                int dCurX = nx - ax;
                int dCurY = ny - ay;
                float distToCurrent = std::sqrt(static_cast<float>(dCurX * dCurX + dCurY * dCurY));

                float diff = std::abs(distToTarget - idealRangeTiles) + distToCurrent * 0.2f;
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestTile = {nx, ny};
                }
            }
        }
    }

    return bestTile;
}

sim::TileCoord TacticalAI::pickScoutingTarget(const sim::WorldSim& sim,
                                              int scoutIndex, int totalScouts,
                                              sim::UnitDomain domain, sim::UnitType type) {
    const auto& map = sim.getMap();
    int mapW = map.getWidth();
    int mapH = map.getHeight();

    int nSectors = std::max(1, totalScouts);
    int sector = scoutIndex % nSectors;

    // Trinh sát hướng về nửa bản đồ đối phương; cự ly thâm nhập phụ thuộc scoutingWeight
    float depthFactor = std::clamp(m_params.scoutingWeight, 0.2f, 2.5f);
    int midW = mapW / 2;
    int baseOffset = static_cast<int>(8.0f + 18.0f * depthFactor);

    int targetX = (m_controlledFaction == sim::Faction::Enemy)
                  ? std::clamp(midW - baseOffset + (scoutIndex % 2) * 8, 4, mapW - 5)
                  : std::clamp(midW + baseOffset - (scoutIndex % 2) * 8, 4, mapW - 5);

    int targetY = (mapH / (nSectors + 1)) * (sector + 1);

    // Tìm ô passable gần đó
    for (int r = 0; r < 20; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int nx = targetX + dx;
                int ny = targetY + dy;
                if (!map.isInBounds(nx, ny)) continue;
                sim::DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, nx, ny);
                if (sim.getConfig().isPassable(dt, domain, type)) {
                    return {nx, ny};
                }
            }
        }
    }

    return {std::clamp(targetX, 2, mapW - 3), std::clamp(targetY, 2, mapH - 3)};
}

const EnemyMemoryRecord* TacticalAI::findHighValueEnemyTarget(sim::UnitDomain targetFilter) const {
    const EnemyMemoryRecord* best = nullptr;
    float maxVal = -1.0f;

    for (const auto& enemy : m_memory) {
        if (targetFilter == sim::UnitDomain::Land && enemy.domain != sim::UnitDomain::Land) continue;
        if (targetFilter == sim::UnitDomain::Sea && enemy.domain != sim::UnitDomain::Sea) continue;
        if (targetFilter == sim::UnitDomain::Air && enemy.domain != sim::UnitDomain::Air) continue;

        float val = 1.0f;
        switch (enemy.type) {
            case sim::UnitType::Commander: val = 10.0f; break;
            case sim::UnitType::Caesar: val = 5.0f; break;
            case sim::UnitType::Leopard2A: val = 4.0f; break;
            case sim::UnitType::T14Armata: val = 3.5f; break;
            case sim::UnitType::Infantry: val = 3.0f; break;
            case sim::UnitType::Puma: val = 2.0f; break;
            default: val = 1.0f; break;
        }

        if (val > maxVal) {
            maxVal = val;
            best = &enemy;
        }
    }

    return best;
}

} // namespace ai
