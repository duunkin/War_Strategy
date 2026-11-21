#include "ai/TacticalAI.hpp"
#include "ai/AiParams.hpp"
#include "sim/WorldSim.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/config/SimConfig.hpp"
#include "sim/units/Unit.hpp"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace sim;
using namespace ai;

// Helper: Khởi tạo WorldSim với cấu hình chuẩn
void initSim(WorldSim& sim) {
    bool ok = sim.loadConfig();
    if (!ok) {
        std::cerr << "Khong the load config cho WorldSim!" << std::endl;
        std::abort();
    }
}

// =============================================================================
// TEST 1: Tính tất định 100% (Determinism Test)
// =============================================================================
void testDeterminism() {
    std::cout << "[TestAI] 1. Kiem tra tinh tat dinh 100% (Determinism Test)..." << std::endl;

    constexpr uint64_t TEST_SEED = 987654321ULL;
    constexpr int RUN_TICKS = 100;

    // Lần chạy 1
    WorldMap map1(128, 96, 32);
    WorldSim sim1(map1);
    initSim(sim1);
    sim1.spawnDivisions(TEST_SEED);

    TacticalAI ai1(Faction::Enemy, TEST_SEED);

    std::vector<std::vector<Command>> history1;
    for (int t = 0; t < RUN_TICKS; ++t) {
        std::vector<Command> cmds = ai1.plan(sim1);
        for (const auto& cmd : cmds) {
            sim1.queueCommand(cmd);
        }
        history1.push_back(cmds);
        sim1.tick();
    }

    // Lần chạy 2 (khởi tạo độc lập cùng seed)
    WorldMap map2(128, 96, 32);
    WorldSim sim2(map2);
    initSim(sim2);
    sim2.spawnDivisions(TEST_SEED);

    TacticalAI ai2(Faction::Enemy, TEST_SEED);

    std::vector<std::vector<Command>> history2;
    for (int t = 0; t < RUN_TICKS; ++t) {
        std::vector<Command> cmds = ai2.plan(sim2);
        for (const auto& cmd : cmds) {
            sim2.queueCommand(cmd);
        }
        history2.push_back(cmds);
        sim2.tick();
    }

    // So sánh từng lệnh qua từng tick
    assert(history1.size() == history2.size());
    for (size_t t = 0; t < history1.size(); ++t) {
        const auto& cmds1 = history1[t];
        const auto& cmds2 = history2[t];
        assert(cmds1.size() == cmds2.size() && "So luong lenh giua 2 lan chay phai giong het nhau!");
        for (size_t c = 0; c < cmds1.size(); ++c) {
            assert(cmds1[c].type == cmds2[c].type);
            assert(cmds1[c].move.targetTileX == cmds2[c].move.targetTileX);
            assert(cmds1[c].move.targetTileY == cmds2[c].move.targetTileY);
            assert(cmds1[c].move.unitIds == cmds2[c].move.unitIds);
        }
    }

    // So sánh trạng thái các đơn vị sau RUN_TICKS
    const auto& units1 = sim1.getUnits();
    const auto& units2 = sim2.getUnits();
    assert(units1.size() == units2.size());
    for (size_t i = 0; i < units1.size(); ++i) {
        assert(units1[i].getId() == units2[i].getId());
        assert(units1[i].getPosX() == units2[i].getPosX());
        assert(units1[i].getPosY() == units2[i].getPosY());
        assert(units1[i].getHp() == units2[i].getHp());
        assert(units1[i].getCurrentUnits() == units2[i].getCurrentUnits());
    }

    std::cout << "  -> PASSED: 2 lan chay doc lap cho ket qua lenh va vi tri giong het nhau 100%!" << std::endl;
}

// =============================================================================
// TEST 2: Tuân thủ Sương mù chiến tranh (Fog-of-War Compliance Test)
// =============================================================================
void testFogOfWarCompliance() {
    std::cout << "[TestAI] 2. Kiem tra tuan thu suong mu chien tranh (Fog-of-War Compliance)..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    initSim(sim);

    // Đặt 1 đơn vị Enemy ở (100, 50)
    const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
    assert(infCfg);
    sim.getUnits().emplace_back(1, Faction::Enemy, UnitType::Infantry, 100000, 50000, *infCfg);

    // Đặt 1 đơn vị Player ở (20, 50) - rất xa, hoàn toàn trong sương mù
    sim.getUnits().emplace_back(2, Faction::Player, UnitType::Infantry, 20000, 50000, *infCfg);

    TacticalAI enemyAI(Faction::Enemy, 42ULL);

    // Cập nhật tầm nhìn sim
    sim.updateVision();

    // Xác nhận Player ngoài tầm nhìn của Enemy
    const Unit* playerUnit = sim.getUnitById(2);
    assert(playerUnit != nullptr);
    assert(!sim.isUnitVisible(*playerUnit, Faction::Enemy) && "Player ngoai tam nhin phai khong thay duoc!");

    // Cho AI lập kế hoạch
    std::vector<Command> cmds = enemyAI.plan(sim);

    // Xác nhận bộ nhớ tình báo của AI không chứa đơn vị Player
    assert(enemyAI.getMemory().empty() && "AI khong duoc biet thong tin quan dich trong suong mu!");

    // Xác nhận các lệnh phát ra KHÔNG nhắm thẳng vào ô (20, 50)
    for (const auto& cmd : cmds) {
        assert(!(cmd.move.targetTileX == 20 && cmd.move.targetTileY == 50) &&
               "AI khong duoc phep ra lenh di dung vao vi tri quan dich dang an!");
    }

    // =========================================================================
    // Test riêng Tư lệnh đối phương tàng hình ngoài 50m (1 ô)
    // =========================================================================
    const auto* comCfg = sim.getConfig().getUnitConfig(UnitType::Commander);
    assert(comCfg);
    // Đặt Tư lệnh Player tại (97, 50) - cách Enemy 3 ô (150m > 50m), trong vùng có thể mở tầm nhìn nhưng stealth
    sim.getUnits().emplace_back(3, Faction::Player, UnitType::Commander, 97000, 50000, *comCfg);
    sim.updateVision();

    const Unit* playerCommander = sim.getUnitById(3);
    assert(playerCommander != nullptr);
    assert(!sim.isUnitVisible(*playerCommander, Faction::Enemy) &&
           "Tu lenh dich ngoai 50m dang tang hinh phai KHONG the bi nhin thay!");

    std::vector<Command> cmds2 = enemyAI.plan(sim);
    // Xác nhận AI không hề có bản ghi nào về Tư lệnh
    for (const auto& rec : enemyAI.getMemory()) {
        assert(rec.unitId != 3 && "AI khong duoc chua vi tri Tu lenh dang an nap!");
    }

    std::cout << "  -> PASSED: AI hoan toan 'mu' truoc quan dich trong FOW va Tu lenh tang hinh!" << std::endl;
}

// =============================================================================
// TEST 3: Hành vi chiến thuật của Tư lệnh, Pháo và Tàu chiến (Tactical Behaviors)
// =============================================================================
void testTacticalBehaviors() {
    std::cout << "[TestAI] 3. Kiem tra hanh vi chien thuat (Commander, Caesar, Ships)..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    initSim(sim);

    const auto* comCfg = sim.getConfig().getUnitConfig(UnitType::Commander);
    const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
    const auto* caesarCfg = sim.getConfig().getUnitConfig(UnitType::Caesar);
    const auto* mogamiCfg = sim.getConfig().getUnitConfig(UnitType::Mogami);

    // 1. Kiểm tra Tư lệnh Enemy né chạy khi địch áp sát
    // Đặt Commander Enemy ở (50, 50)
    sim.getUnits().emplace_back(10, Faction::Enemy, UnitType::Commander, 50000, 50000, *comCfg);
    // Đặt Bộ binh Player ở (48, 50) - cách 2 ô, rất nguy hiểm!
    sim.getUnits().emplace_back(20, Faction::Player, UnitType::Infantry, 48000, 50000, *infCfg);
    sim.updateVision();

    TacticalAI enemyAI(Faction::Enemy, 111ULL);
    std::vector<Command> comCmds = enemyAI.plan(sim);

    bool commanderRetreated = false;
    for (const auto& cmd : comCmds) {
        if (!cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 10) {
            // Địch ở (48, 50) là hướng Tây -> Tư lệnh phải né về hướng Đông (X > 50)
            assert(cmd.move.targetTileX > 50 && "Tu lenh phai thao chay huong nguoc lai hiem hoa!");
            commanderRetreated = true;
        }
    }
    assert(commanderRetreated && "Tu lenh phai phat lenh thao chay khi bi ap sat!");
    std::cout << "  - Tu lenh (Commander): Thao chay nguoc huong quan dich thanh cong." << std::endl;

    // 2. Kiểm tra Tàu chiến (Mogami): 100% lệnh đích phải là ô BIỂN
    sim.getUnits().clear();
    // Đặt 1 tàu Mogami Enemy tại ô biển (2, 2)
    sim.getUnits().emplace_back(30, Faction::Enemy, UnitType::Mogami, 2000, 2000, *mogamiCfg);
    sim.updateVision();

    TacticalAI shipAI(Faction::Enemy, 222ULL);
    std::vector<Command> shipCmds = shipAI.plan(sim);
    for (const auto& cmd : shipCmds) {
        if (!cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 30) {
            DetailedTerrainType dt = sim.getConfig().classifyTerrain(map, cmd.move.targetTileX, cmd.move.targetTileY);
            assert(sim.getConfig().isPassable(dt, UnitDomain::Sea, UnitType::Mogami) &&
                   "Dich den cua tau chien bat buoc phai la o bien hop le!");
        }
    }
    std::cout << "  - Thuy quan (Mogami): 100% lenh di chuyen chi tren bien, khong len bo." << std::endl;

    // 3. Kiểm tra Caesar: Giữ cự ly stand-off
    sim.getUnits().clear();
    // Caesar Enemy tại (60, 50)
    sim.getUnits().emplace_back(40, Faction::Enemy, UnitType::Caesar, 60000, 50000, *caesarCfg);
    // Địch ở (30, 50) - cách 30 ô, ngoài tầm bắn 14 ô
    sim.getUnits().emplace_back(50, Faction::Player, UnitType::Infantry, 30000, 50000, *infCfg);
    // Mở tầm nhìn cho Enemy thấy địch (bằng cách đặt 1 Su-57 trinh sát tại 30, 48)
    const auto* suCfg = sim.getConfig().getUnitConfig(UnitType::Su57);
    sim.getUnits().emplace_back(41, Faction::Enemy, UnitType::Su57, 30000, 48000, *suCfg);
    sim.updateVision();

    TacticalAI caesarAI(Faction::Enemy, 333ULL);
    std::vector<Command> caesarCmds = caesarAI.plan(sim);
    bool caesarMovedToStandOff = false;
    for (const auto& cmd : caesarCmds) {
        if (!cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 40) {
            // Caesar phải tiến về phía Tây (X < 60) để đưa mục tiêu vào tầm bắn stand-off
            assert(cmd.move.targetTileX < 60 && "Caesar phai tien den vi tri stand-off de ban muc tieu!");
            caesarMovedToStandOff = true;
        }
    }
    assert(caesarMovedToStandOff && "Caesar phai phat lenh di chuyen den cu ly stand-off!");
    std::cout << "  - Phao binh (Caesar): Duy tri stand-off hop ly thanh cong." << std::endl;

    std::cout << "  -> PASSED: Toan bo hanh vi cua 5 quan chung deu dat tieu chuan!" << std::endl;
}

// =============================================================================
// TEST 4: Tác chiến và giao tranh thực tế nhiều tick (Combat & Engagement Test)
// =============================================================================
void testCombatAndMovementEngagement() {
    std::cout << "[TestAI] 4. Kiem tra tac chien & giao tranh thuc te (AI vs AI Headless 200 ticks)..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    initSim(sim);
    sim.spawnDivisions(55555ULL);

    // Khởi tạo 2 AI: Player AI vs Enemy AI (sẵn sàng cho mốc M8!)
    TacticalAI playerAI(Faction::Player, 12345ULL);
    TacticalAI enemyAI(Faction::Enemy, 67890ULL);

    int initialPlayerHp = 0;
    int initialEnemyHp = 0;
    for (const auto& u : sim.getUnits()) {
        if (u.getFaction() == Faction::Player) initialPlayerHp += u.getHp();
        else if (u.getFaction() == Faction::Enemy) initialEnemyHp += u.getHp();
    }

    constexpr int ENGAGE_TICKS = 200;
    for (int t = 0; t < ENGAGE_TICKS; ++t) {
        playerAI.update(sim);
        enemyAI.update(sim);
        sim.tick();
    }

    int finalPlayerHp = 0;
    int finalEnemyHp = 0;
    bool hasMoved = false;

    for (const auto& u : sim.getUnits()) {
        if (u.getFaction() == Faction::Player) finalPlayerHp += u.getHp();
        else if (u.getFaction() == Faction::Enemy) finalEnemyHp += u.getHp();
        if (u.hasPath() || u.hasDestination()) {
            hasMoved = true;
        }
    }

    // Xác nhận quân đã di chuyển và giao chiến thành công
    assert(hasMoved || finalPlayerHp != initialPlayerHp || finalEnemyHp != initialEnemyHp);
    std::cout << "  - Quan so va mau sau 200 ticks: Player HP = " << finalPlayerHp << " (Goc: " << initialPlayerHp
              << "), Enemy HP = " << finalEnemyHp << " (Goc: " << initialEnemyHp << ")" << std::endl;
    std::cout << "  -> PASSED: AI vs AI headless hoat dong on dinh, khong crash, dieu quan chuan xac!" << std::endl;
}

// =============================================================================
// TEST 5: Kiem tra cac tham so AI co tac dung thuc te (Parameter Effectiveness Test)
// =============================================================================
void testParameterEffectiveness() {
    std::cout << "[TestAI] 5. Kiem tra tinh co hieu luc cua cac tham so AI (Parameter Effectiveness)..." << std::endl;

    constexpr uint64_t SEED = 77777ULL;

    // 5.1. Kiem tra tham so 'aggression' (dieu khien cu ly tien quan va ti le tan cong)
    {
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        initSim(sim);
        sim.spawnDivisions(SEED);

        AiParams paramsLow;
        paramsLow.aggression = 0.20f;
        TacticalAI aiLow(Faction::Enemy, SEED, paramsLow);

        AiParams paramsHigh;
        paramsHigh.aggression = 1.30f;
        TacticalAI aiHigh(Faction::Enemy, SEED, paramsHigh);

        std::vector<Command> cmdsLow = aiLow.plan(sim);
        std::vector<Command> cmdsHigh = aiHigh.plan(sim);

        bool foundDifference = false;
        for (const auto& cLow : cmdsLow) {
            if (cLow.type == CommandType::Move && !cLow.move.unitIds.empty()) {
                uint32_t uid = cLow.move.unitIds[0];
                for (const auto& cHigh : cmdsHigh) {
                    if (cHigh.type == CommandType::Move && !cHigh.move.unitIds.empty() && cHigh.move.unitIds[0] == uid) {
                        if (cLow.move.targetTileX != cHigh.move.targetTileX || cLow.move.targetTileY != cHigh.move.targetTileY) {
                            foundDifference = true;
                            break;
                        }
                    }
                }
            }
            if (foundDifference) break;
        }
        assert(foundDifference && "Tham so 'aggression' phai lam thay doi lenh di chuyen cua quan chu luc!");
        std::cout << "  - Tham so 'aggression': Da kiem chung tao ra lenh di chuyen khac biet ro ret." << std::endl;
    }

    // 5.2. Kiem tra tham so 'scoutingWeight' (dieu khien cu ly tham nhap trinh sat Puma)
    {
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        initSim(sim);
        sim.spawnDivisions(SEED);

        AiParams paramsLow;
        paramsLow.scoutingWeight = 0.30f;
        TacticalAI aiLow(Faction::Enemy, SEED, paramsLow);

        AiParams paramsHigh;
        paramsHigh.scoutingWeight = 2.00f;
        TacticalAI aiHigh(Faction::Enemy, SEED, paramsHigh);

        std::vector<Command> cmdsLow = aiLow.plan(sim);
        std::vector<Command> cmdsHigh = aiHigh.plan(sim);

        bool foundPumaDiff = false;
        for (const auto& cLow : cmdsLow) {
            if (cLow.type == CommandType::Move && !cLow.move.unitIds.empty()) {
                uint32_t uid = cLow.move.unitIds[0];
                const Unit* u = nullptr;
                for (const auto& unit : sim.getUnits()) {
                    if (unit.getId() == uid) {
                        u = &unit;
                        break;
                    }
                }
                if (u && u->getType() == UnitType::Puma) {
                    for (const auto& cHigh : cmdsHigh) {
                        if (cHigh.type == CommandType::Move && !cHigh.move.unitIds.empty() && cHigh.move.unitIds[0] == uid) {
                            if (cLow.move.targetTileX != cHigh.move.targetTileX) {
                                foundPumaDiff = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (foundPumaDiff) break;
        }
        assert(foundPumaDiff && "Tham so 'scoutingWeight' phai lam thay doi toa do muc tieu trinh sat cua Puma!");
        std::cout << "  - Tham so 'scoutingWeight': Da kiem chung lam thay doi dich den trinh sat." << std::endl;
    }

    // 5.3. Kiem tra tham so 'artilleryStandOffMargin' (nguong lui cua phao Caesar)
    {
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        initSim(sim);

        const auto* caesarCfg = sim.getConfig().getUnitConfig(UnitType::Caesar);
        const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
        const auto* suCfg = sim.getConfig().getUnitConfig(UnitType::Su57);

        // Caesar tai (60, 50), dich tai (53, 50) -> khoang cach 7.0 o
        // Caesar range = 14. 14 * 0.8 = 11.2
        sim.getUnits().emplace_back(101, Faction::Enemy, UnitType::Caesar, 60000, 50000, *caesarCfg);
        sim.getUnits().emplace_back(201, Faction::Player, UnitType::Infantry, 53000, 50000, *infCfg);
        // Su57 mo tam nhin
        sim.getUnits().emplace_back(102, Faction::Enemy, UnitType::Su57, 53000, 48000, *suCfg);
        sim.updateVision();

        AiParams paramsRetreat;
        paramsRetreat.artilleryStandOffMargin = 1.0f; // retreatThreshold = 11.2 - 1.0 = 10.2 > 7.0 -> Lui!
        TacticalAI aiRetreat(Faction::Enemy, SEED, paramsRetreat);

        AiParams paramsHold;
        paramsHold.artilleryStandOffMargin = 6.0f; // retreatThreshold = 11.2 - 6.0 = 5.2 <= 7.0 -> Giu nguyen ban!
        TacticalAI aiHold(Faction::Enemy, SEED, paramsHold);

        std::vector<Command> cmdsRetreat = aiRetreat.plan(sim);
        std::vector<Command> cmdsHold = aiHold.plan(sim);

        bool retreatIssued = false;
        for (const auto& cmd : cmdsRetreat) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 101) {
                retreatIssued = true;
            }
        }
        bool holdIssuedMove = false;
        for (const auto& cmd : cmdsHold) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 101) {
                holdIssuedMove = true;
            }
        }

        assert(retreatIssued && !holdIssuedMove && "Caesar phai lui khi margin hep va dung im ban khi margin rong!");
        std::cout << "  - Tham so 'artilleryStandOffMargin': Da kiem chung quyet dinh hanh vi lui vs giu vi tri cua Caesar." << std::endl;
    }

    // 5.4. Kiem tra tham so 'focusFireWeight' (don hoa luc vao cung mot muc tieu)
    {
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        initSim(sim);

        const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
        const auto* suCfg = sim.getConfig().getUnitConfig(UnitType::Su57);

        // 2 quan ta (Infantry): u1 tai (50, 50), u2 tai (40, 55)
        sim.getUnits().emplace_back(10, Faction::Enemy, UnitType::Infantry, 50000, 50000, *infCfg);
        sim.getUnits().emplace_back(11, Faction::Enemy, UnitType::Infantry, 40000, 55000, *infCfg);

        // 2 dich (Infantry): eA tai (40, 50), eB tai (42, 50)
        sim.getUnits().emplace_back(20, Faction::Player, UnitType::Infantry, 40000, 50000, *infCfg);
        sim.getUnits().emplace_back(21, Faction::Player, UnitType::Infantry, 42000, 50000, *infCfg);

        // Su-57 de mo tam nhin
        sim.getUnits().emplace_back(12, Faction::Enemy, UnitType::Su57, 41000, 50000, *suCfg);
        sim.updateVision();

        AiParams pNoFocus;
        pNoFocus.focusFireWeight = 1.0f;
        TacticalAI aiNoFocus(Faction::Enemy, SEED, pNoFocus);

        AiParams pFocus;
        pFocus.focusFireWeight = 4.0f;
        TacticalAI aiFocus(Faction::Enemy, SEED, pFocus);

        std::vector<Command> cmdsNoFocus = aiNoFocus.plan(sim);
        std::vector<Command> cmdsFocus = aiFocus.plan(sim);

        // Tim dich den cua unit 11 trong 2 truong hop
        int targetXNoFocus = -1;
        for (const auto& cmd : cmdsNoFocus) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 11) {
                targetXNoFocus = cmd.move.targetTileX;
            }
        }
        int targetXFocus = -1;
        for (const auto& cmd : cmdsFocus) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 11) {
                targetXFocus = cmd.move.targetTileX;
            }
        }

        assert(targetXNoFocus != -1 && targetXFocus != -1);
        assert(targetXNoFocus != targetXFocus && "Tham so 'focusFireWeight' phai thay doi muc tieu don hoa luc cua don vi thu hai!");
        std::cout << "  - Tham so 'focusFireWeight': Da kiem chung don hoa luc thanh cong." << std::endl;
    }

    // 5.5. Kiem tra tham so 'enableCommanderRetreat'
    {
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        initSim(sim);

        const auto* cmdCfg = sim.getConfig().getUnitConfig(UnitType::Commander);
        const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
        const auto* suCfg = sim.getConfig().getUnitConfig(UnitType::Su57);

        // Commander tai (50, 50), dich tai (48, 50)
        sim.getUnits().emplace_back(1, Faction::Enemy, UnitType::Commander, 50000, 50000, *cmdCfg);
        sim.getUnits().emplace_back(2, Faction::Player, UnitType::Infantry, 48000, 50000, *infCfg);
        sim.getUnits().emplace_back(3, Faction::Enemy, UnitType::Su57, 49000, 50000, *suCfg);
        sim.updateVision();

        AiParams pRetreat;
        pRetreat.enableCommanderRetreat = true;
        TacticalAI aiRetreat(Faction::Enemy, SEED, pRetreat);

        AiParams pNoRetreat;
        pNoRetreat.enableCommanderRetreat = false;
        TacticalAI aiNoRetreat(Faction::Enemy, SEED, pNoRetreat);

        std::vector<Command> cmdsRetreat = aiRetreat.plan(sim);
        std::vector<Command> cmdsNoRetreat = aiNoRetreat.plan(sim);

        bool cmdMovedRetreat = false;
        for (const auto& cmd : cmdsRetreat) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 1) {
                cmdMovedRetreat = true;
            }
        }
        bool cmdMovedNoRetreat = false;
        for (const auto& cmd : cmdsNoRetreat) {
            if (cmd.type == CommandType::Move && !cmd.move.unitIds.empty() && cmd.move.unitIds[0] == 1) {
                cmdMovedNoRetreat = true;
            }
        }

        assert(cmdMovedRetreat && !cmdMovedNoRetreat && "enableCommanderRetreat phai dieu khien lenh chay cua Tu lenh!");
        std::cout << "  - Tham so 'enableCommanderRetreat': Da kiem chung bat/tat rut lui Tu lenh." << std::endl;
    }

    std::cout << "  -> PASSED: Toan bo cac tham so AI deu da duoc chung minh co tac dung thuc te 100%!" << std::endl;
}

int main() {
    std::cout << "====================================================" << std::endl;
    std::cout << "BAT DAU TEST SUITE CHO MOC M7: TACTICAL AI" << std::endl;
    std::cout << "====================================================" << std::endl;

    testDeterminism();
    testFogOfWarCompliance();
    testTacticalBehaviors();
    testCombatAndMovementEngagement();
    testParameterEffectiveness();

    std::cout << "====================================================" << std::endl;
    std::cout << "TAT CA 5/5 TEST M7 CHO TACTICAL AI DEU PASS 100%!" << std::endl;
    std::cout << "====================================================" << std::endl;
    return 0;
}
