#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

#include "sim/combat/CombatSystem.hpp"
#include "sim/config/SimConfig.hpp"
#include "sim/units/Unit.hpp"
#include "sim/WorldSim.hpp"
#include "sim/map/WorldMap.hpp"

// =============================================================================
// KHÓA CÔNG THỨC: Kiểm tra các giá trị sát thương đã duyệt theo bảng TTK
// =============================================================================
void testFormulaLocking(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Khoa cong thuc] Kiem tra cac gia tri sat thuong da duyet..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);
    const auto* cfgT14 = config.getUnitConfig(sim::UnitType::T14Armata);
    const auto* cfgSu = config.getUnitConfig(sim::UnitType::Su57);
    const auto* cfgMogami = config.getUnitConfig(sim::UnitType::Mogami);
    const auto* cfgAsagiri = config.getUnitConfig(sim::UnitType::Asagiri);

    assert(cfgInf && cfgLeo && cfgT14 && cfgSu && cfgMogami && cfgAsagiri);

    // 1. Bộ binh bắn bộ binh: 8.424 mỗi lượt đầy quân
    int64_t dmgInfToInf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], cfgInf->unitsPerDot,
        cfgInf->armor, cfgInf->domain, false
    );
    std::cout << "  - Bo binh -> Bo binh: " << dmgInfToInf << " (ky vong 8424)" << std::endl;
    assert(dmgInfToInf == 8424);

    // 2. Bộ binh bắn Leopard 2A: 820 mỗi lượt đầy quân
    int64_t dmgInfToLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], cfgInf->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, false
    );
    std::cout << "  - Bo binh -> Leopard 2A: " << dmgInfToLeo << " (ky vong 820)" << std::endl;
    assert(dmgInfToLeo == 820);

    // 3. Leopard 2A bắn bộ binh: tự chọn HE đạt 9.022 mỗi lượt đầy quân
    size_t leoWeaponIdx = sim::CombatSystem::chooseBestWeaponIndex(
        sim::UnitType::Leopard2A, cfgLeo->weapons, cfgLeo->unitsPerDot,
        cfgInf->armor, cfgInf->domain
    );
    assert(cfgLeo->weapons[leoWeaponIdx].ammo == "HE");
    int64_t dmgLeoToInf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Leopard2A, cfgLeo->weapons[leoWeaponIdx], cfgLeo->unitsPerDot,
        cfgInf->armor, cfgInf->domain, false
    );
    std::cout << "  - Leopard 2A -> Bo binh (HE): " << dmgLeoToInf << " (ky vong 9022)" << std::endl;
    assert(dmgLeoToInf == 9022);

    // 4. T-14 Armata bắn Leopard 2A: tự chọn AP đạt 4.104 mỗi lượt đầy quân
    size_t t14WeaponIdx = sim::CombatSystem::chooseBestWeaponIndex(
        sim::UnitType::T14Armata, cfgT14->weapons, cfgT14->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain
    );
    assert(cfgT14->weapons[t14WeaponIdx].ammo == "AP");
    int64_t dmgT14ToLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::T14Armata, cfgT14->weapons[t14WeaponIdx], cfgT14->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, false
    );
    std::cout << "  - T-14 -> Leopard 2A (AP): " << dmgT14ToLeo << " (ky vong 4104)" << std::endl;
    assert(dmgT14ToLeo == 4104);

    // 5. Su-57 bắn Leopard 2A: 1.641 (kỳ vọng, accuracy 0.6)
    int64_t dmgSuToLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Su57, cfgSu->weapons[0], cfgSu->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, false
    );
    std::cout << "  - Su-57 -> Leopard 2A (AP, acc 0.6): " << dmgSuToLeo << " (ky vong 1641)" << std::endl;
    assert(dmgSuToLeo == 1641);

    // 6. Mogami bắn Asagiri chỉ bằng HE: 690 mỗi lượt đầy quân
    const sim::WeaponConfig* mogamiHe = nullptr;
    for (const auto& w : cfgMogami->weapons) {
        if (w.ammo == "HE") {
            mogamiHe = &w;
            break;
        }
    }
    assert(mogamiHe != nullptr);
    int64_t dmgMogamiHeToAsagiri = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Mogami, *mogamiHe, cfgMogami->unitsPerDot,
        cfgAsagiri->armor, cfgAsagiri->domain, false
    );
    std::cout << "  - Mogami -> Asagiri (chi tinh HE): " << dmgMogamiHeToAsagiri << " (ky vong 690)" << std::endl;
    assert(dmgMogamiHeToAsagiri == 690);

    std::cout << "-> PASSED Khoa cong thuc sat thuong!\n" << std::endl;
}

// =============================================================================
// MỤC 1: Chỉ tàu bắn được tàu; bộ binh, xe, pháo, máy bay không đánh được tàu
// =============================================================================
void testShipTargetingOnlyShips(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Muc 1] Kiem tra: Chi tau ban duoc tau; bo binh, xe, phao, may bay khong danh duoc tau..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);
    const auto* cfgPuma = config.getUnitConfig(sim::UnitType::Puma);
    const auto* cfgCaesar = config.getUnitConfig(sim::UnitType::Caesar);
    const auto* cfgGepard = config.getUnitConfig(sim::UnitType::Gepard);
    const auto* cfgSu = config.getUnitConfig(sim::UnitType::Su57);
    const auto* cfgB2 = config.getUnitConfig(sim::UnitType::B2Spirit);
    const auto* cfgMogami = config.getUnitConfig(sim::UnitType::Mogami);
    const auto* cfgAsagiri = config.getUnitConfig(sim::UnitType::Asagiri);

    // Đặt tàu địch tại (11000, 10000)
    sim::Unit shipEnemy(1, sim::Faction::Enemy, sim::UnitType::Asagiri, 11000, 10000, *cfgAsagiri);

    // Đặt các đơn vị phe ta tại (10000, 10000) - trong tầm bắn nếu được phép
    sim::Unit infPlayer(2, sim::Faction::Player, sim::UnitType::Infantry, 10000, 10000, *cfgInf);
    sim::Unit leoPlayer(3, sim::Faction::Player, sim::UnitType::Leopard2A, 10000, 10000, *cfgLeo);
    sim::Unit pumaPlayer(4, sim::Faction::Player, sim::UnitType::Puma, 10000, 10000, *cfgPuma);
    sim::Unit caesarPlayer(5, sim::Faction::Player, sim::UnitType::Caesar, 10000, 10000, *cfgCaesar);
    sim::Unit gepardPlayer(6, sim::Faction::Player, sim::UnitType::Gepard, 10000, 10000, *cfgGepard);
    sim::Unit suPlayer(7, sim::Faction::Player, sim::UnitType::Su57, 10000, 10000, *cfgSu);
    sim::Unit b2Player(8, sim::Faction::Player, sim::UnitType::B2Spirit, 10000, 10000, *cfgB2);
    sim::Unit shipPlayer(9, sim::Faction::Player, sim::UnitType::Mogami, 10000, 10000, *cfgMogami);

    // 1.1. Bộ binh KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(infPlayer, shipEnemy));
    // 1.2. Xe tăng KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(leoPlayer, shipEnemy));
    // 1.3. Xe chiến đấu (Puma) KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(pumaPlayer, shipEnemy));
    // 1.4. Pháo tự hành (Caesar) KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(caesarPlayer, shipEnemy));
    // 1.5. Pháo phòng không (Gepard) KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(gepardPlayer, shipEnemy));
    // 1.6. Máy bay tiêm kích (Su-57) KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(suPlayer, shipEnemy));
    // 1.7. Máy bay ném bom (B-2 Spirit) KHÔNG đánh được tàu
    assert(!sim::CombatSystem::canAttackTarget(b2Player, shipEnemy));

    // 1.8. DUY NHẤT TÀU CHIẾN MỚI BẮN ĐƯỢC TÀU CHIẾN
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, shipEnemy));

    std::cout << "-> PASSED Muc 1: Chi tau ban duoc tau; cac luc luong khac khong the danh tau!\n" << std::endl;
}

// =============================================================================
// MỤC 2: Tàu bắn được vào bờ, không bắn được máy bay
// =============================================================================
void testShipBombardsShoreNotAir(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Muc 2] Kiem tra: Tau ban duoc vao bo, khong ban duoc may bay..." << std::endl;

    const auto* cfgMogami = config.getUnitConfig(sim::UnitType::Mogami);
    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);
    const auto* cfgSu = config.getUnitConfig(sim::UnitType::Su57);
    const auto* cfgB2 = config.getUnitConfig(sim::UnitType::B2Spirit);

    sim::Unit shipPlayer(1, sim::Faction::Player, sim::UnitType::Mogami, 10000, 10000, *cfgMogami);

    // Mục tiêu trên bờ (Land): Bộ binh và Xe tăng
    sim::Unit infEnemy(2, sim::Faction::Enemy, sim::UnitType::Infantry, 11000, 10000, *cfgInf);
    sim::Unit leoEnemy(3, sim::Faction::Enemy, sim::UnitType::Leopard2A, 11000, 10000, *cfgLeo);

    // Mục tiêu trên không (Air): Su-57 và B-2
    sim::Unit suEnemy(4, sim::Faction::Enemy, sim::UnitType::Su57, 11000, 10000, *cfgSu);
    sim::Unit b2Enemy(5, sim::Faction::Enemy, sim::UnitType::B2Spirit, 11000, 10000, *cfgB2);

    // 2.1. Tàu bắn được vào bờ
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, infEnemy));
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, leoEnemy));

    // 2.2. Tàu KHÔNG bắn được máy bay
    assert(!sim::CombatSystem::canAttackTarget(shipPlayer, suEnemy));
    assert(!sim::CombatSystem::canAttackTarget(shipPlayer, b2Enemy));

    std::cout << "-> PASSED Muc 2: Tau ban duoc vao bo, khong ban duoc may bay!\n" << std::endl;
}

// =============================================================================
// MỤC 3: Sát thương nổ lan tính theo giáp từng chấm bên cạnh, không lan sang quân mình
// =============================================================================
void testSplashDamageAndNoFriendlyFire(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Muc 3] Kiem tra: Sat thuong no lan tinh theo giap tung cham ben canh, khong lan sang quan minh..." << std::endl;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    sim.loadConfig();

    const auto* cfgCaesar = config.getUnitConfig(sim::UnitType::Caesar);
    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);

    // Caesar (Player) tại (10000, 10000), tầm 14 ô
    sim::Unit caesar(1, sim::Faction::Player, sim::UnitType::Caesar, 10000, 10000, *cfgCaesar);
    // Mục tiêu chính: Bộ binh địch tại (15000, 10000) (cách 5 ô)
    sim::Unit targetEnemy(2, sim::Faction::Enemy, sim::UnitType::Infantry, 15000, 10000, *cfgInf);
    // Chấm địch 2: Leopard 2A địch (giáp 68) tại (15500, 10000) (cách mục tiêu 0.5 ô, trong splash 1.5 ô)
    sim::Unit adjacentEnemy(3, sim::Faction::Enemy, sim::UnitType::Leopard2A, 15500, 10000, *cfgLeo);
    // Chấm ĐỒNG MINH (Player) tại (14500, 10000) (cách mục tiêu 0.5 ô, trong splash 1.5 ô)
    sim::Unit adjacentFriend(4, sim::Faction::Player, sim::UnitType::Infantry, 14500, 10000, *cfgInf);

    // Đặt cooldown của các đơn vị địch > 0 để chúng không bắn trả trong tick này
    targetEnemy.setCooldownRemaining(100);
    adjacentEnemy.setCooldownRemaining(100);

    int targetHpBefore = targetEnemy.getHp();
    int enemyLeoHpBefore = adjacentEnemy.getHp();
    int friendHpBefore = adjacentFriend.getHp();

    sim.getUnits() = {caesar, targetEnemy, adjacentEnemy, adjacentFriend};
    sim.resolveCombat();

    int targetHpAfter = sim.getUnits()[1].getHp();
    int enemyLeoHpAfter = sim.getUnits()[2].getHp();
    int friendHpAfter = sim.getUnits()[3].getHp();

    int targetDmg = targetHpBefore - targetHpAfter;
    int enemyLeoDmg = enemyLeoHpBefore - enemyLeoHpAfter;
    int friendDmg = friendHpBefore - friendHpAfter;

    std::cout << "  - Muc tieu chinh (Bo binh dich giap 8) chiu: " << targetDmg << " dmg" << std::endl;
    std::cout << "  - Cham ben canh (Leopard dich giap 68) chiu: " << enemyLeoDmg << " dmg (tinh theo giap 68)" << std::endl;
    std::cout << "  - Cham dong minh ben canh chiu: " << friendDmg << " dmg" << std::endl;

    // Mục tiêu chính nhận sát thương trực tiếp
    assert(targetDmg > 0);
    // Chấm địch bên cạnh nhận sát thương nổ lan (tính theo giáp 68)
    assert(enemyLeoDmg > 0 && enemyLeoDmg < targetDmg);
    // Chấm đồng minh TUYỆT ĐỐI KHÔNG BỊ SÁT THƯƠNG LAN (Không friendly fire)
    assert(friendDmg == 0);
    assert(friendHpAfter == friendHpBefore);

    std::cout << "-> PASSED Muc 3: Sat thuong lan tinh theo giap tung cham va khong lan sang quan minh!\n" << std::endl;
}

// =============================================================================
// MỤC 4: Thương vong làm giảm số phát bắn ở lượt sau
// =============================================================================
void testCasualtiesReduceFutureFirepower(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Muc 4] Kiem tra: Thuong vong lam giam so phat ban o luot sau..." << std::endl;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    sim.loadConfig();

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);

    // Chấm tấn công: Bộ binh Player đầy quân (3000 lính)
    sim::Unit attacker(1, sim::Faction::Player, sim::UnitType::Infantry, 10000, 10000, *cfgInf);
    // Mục tiêu bia tập bắn (cooldown > 0 để không bắn trả)
    sim::Unit dummy(2, sim::Faction::Enemy, sim::UnitType::Infantry, 12000, 10000, *cfgInf);
    dummy.setCooldownRemaining(100);

    sim.getUnits() = {attacker, dummy};

    // Lượt 1: Attacker còn đủ 3000 lính bắn bia
    sim.resolveCombat();
    int hpAfterVolley1 = sim.getUnits()[1].getHp();
    int dmgVolley1 = dummy.getMaxHp() - hpAfterVolley1;
    std::cout << "  - Luot 1 (3000 linh ban): gay " << dmgVolley1 << " dmg" << std::endl;
    assert(dmgVolley1 == 8424);

    // Khiến attacker chịu thương vong, trừ 50% quân số (còn 1500 lính)
    sim.getUnits()[0].takeDamage(150000); // 1500 lính x 100 hp = 150000 hp
    assert(sim.getUnits()[0].getCurrentUnits() == 1500);

    // Đặt cooldown = 0 để attacker bắn tiếp lượt 2
    sim.getUnits()[0].setCooldownRemaining(0);
    sim.resolveCombat();
    int hpAfterVolley2 = sim.getUnits()[1].getHp();
    int dmgVolley2 = hpAfterVolley1 - hpAfterVolley2;
    std::cout << "  - Luot 2 (sau thuong vong, con 1500 linh ban): gay " << dmgVolley2 << " dmg" << std::endl;
    assert(dmgVolley2 == 4212); // Đúng một nửa của lượt 1
    assert(dmgVolley2 == dmgVolley1 / 2);

    std::cout << "-> PASSED Muc 4: Thuong vong lam giam so phat ban o luot sau!\n" << std::endl;
}

// =============================================================================
// MỤC 5: Thua khi tư lệnh chết; thắng khi địch chỉ còn tư lệnh; hòa khi hai bên cùng hết quân
// =============================================================================
void testCommanderAndEliminationVictoryConditions() {
    std::cout << "[TestCombat] [Muc 5] Kiem tra: Thua khi tu lenh chet; thang khi dich chi con tu lenh; hoa khi hai ben cung het quan..." << std::endl;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    sim.loadConfig();

    // 5.1. Thua khi tư lệnh Player chết
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Player && u.getType() == sim::UnitType::Commander) {
            u.takeDamage(999999);
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Defeat);
    std::cout << "  - Tu lenh Player chet -> Defeat: OK" << std::endl;

    // 5.2. Thắng khi tư lệnh Enemy chết
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Enemy && u.getType() == sim::UnitType::Commander) {
            u.takeDamage(999999);
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Victory);
    std::cout << "  - Tu lenh Enemy chet -> Victory: OK" << std::endl;

    // 5.3. Thắng khi địch chỉ còn tư lệnh (tất cả quân khác của địch đã chết, ta còn quân)
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Enemy && u.getType() != sim::UnitType::Commander) {
            u.takeDamage(999999);
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Victory);
    std::cout << "  - Dich het quan (chi con tu lenh) -> Victory: OK" << std::endl;

    // 5.4. Hòa khi hai bên cùng hết quân (cả 2 bên đều chết hết hoặc cả 2 chỉ còn tư lệnh)
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        u.takeDamage(999999);
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Draw);
    std::cout << "  - Hai ben deu chet het quan -> Draw: OK" << std::endl;

    // 5.5. Hòa khi cả 2 tư lệnh cùng chết
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getType() == sim::UnitType::Commander) {
            u.takeDamage(999999);
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Draw);
    std::cout << "  - Ca 2 tu lenh cung chet -> Draw: OK" << std::endl;

    std::cout << "-> PASSED Muc 5: Cac dieu kien tu lenh va diet sach quan!\n" << std::endl;
}

// =============================================================================
// MỤC 6: Hết giờ thì bên có tổng quân gộp nhiều hơn thắng, bằng nhau thì hòa
// =============================================================================
void testTimeoutVictoryConditions() {
    std::cout << "[TestCombat] [Muc 6] Kiem tra: Het gio thi ben co tong quan gop nhieu hon thang, bang nhau thi hoa..." << std::endl;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    sim.loadConfig();

    uint64_t maxTicks = sim.getMatchDurationTicks();

    // 6.1. Hết giờ: Player nhiều quân hơn Enemy -> Player thắng (Victory)
    sim.spawnDivisions(1234ULL);
    // Trừ 1 chấm bộ binh của Enemy (Enemy mất 3000 quân)
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Enemy && u.getType() == sim::UnitType::Infantry) {
            u.takeDamage(999999);
            break;
        }
    }
    sim.setCurrentTick(maxTicks);
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Victory);
    std::cout << "  - Het gio, Player nhieu quan hon -> Victory: OK" << std::endl;

    // 6.2. Hết giờ: Enemy nhiều quân hơn Player -> Player thua (Defeat)
    sim.spawnDivisions(1234ULL);
    // Trừ 1 chấm bộ binh của Player (Player mất 3000 quân)
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Player && u.getType() == sim::UnitType::Infantry) {
            u.takeDamage(999999);
            break;
        }
    }
    sim.setCurrentTick(maxTicks);
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Defeat);
    std::cout << "  - Het gio, Enemy nhieu quan hon -> Defeat: OK" << std::endl;

    // 6.3. Hết giờ: Quân hai bên bằng nhau -> Hòa (Draw)
    sim.spawnDivisions(1234ULL);
    sim.setCurrentTick(maxTicks);
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Draw);
    std::cout << "  - Het gio, quan hai ben can bang -> Draw: OK" << std::endl;

    std::cout << "-> PASSED Muc 6: Cac dieu kien het gio tran dau!\n" << std::endl;
}

// =============================================================================
// MỤC 7: Cùng seed chạy hai lần cho kết quả giống hệt nhau (Determinism)
// =============================================================================
void testSimulationDeterminism() {
    std::cout << "[TestCombat] [Muc 7] Kiem tra: Cung seed chay hai lan cho ket qua giong het nhau..." << std::endl;

    sim::WorldMap map1(128, 96, 32);
    sim::WorldSim sim1(map1);
    sim1.loadConfig();

    sim::WorldMap map2(128, 96, 32);
    sim::WorldSim sim2(map2);
    sim2.loadConfig();

    uint64_t testSeed = 987654ULL;
    sim1.spawnDivisions(testSeed);
    sim2.spawnDivisions(testSeed);

    // Di chuyển các đơn vị Player và Enemy vào trung tâm chạm trán nhau
    std::vector<uint32_t> pIds = {sim1.getUnits()[1].getId(), sim1.getUnits()[11].getId()};
    std::vector<uint32_t> eIds = {sim1.getUnits()[31].getId(), sim1.getUnits()[41].getId()};
    sim1.queueMoveCommand(pIds, 64, 48);
    sim1.queueMoveCommand(eIds, 64, 48);
    sim2.queueMoveCommand(pIds, 64, 48);
    sim2.queueMoveCommand(eIds, 64, 48);

    // Chạy 100 ticks mô phỏng đầy đủ (di chuyển + chiến đấu)
    for (int t = 0; t < 100; ++t) {
        sim1.tick();
        sim2.tick();
    }

    assert(sim1.getUnits().size() == sim2.getUnits().size());
    for (size_t i = 0; i < sim1.getUnits().size(); ++i) {
        const auto& u1 = sim1.getUnits()[i];
        const auto& u2 = sim2.getUnits()[i];
        assert(u1.getId() == u2.getId());
        assert(u1.getHp() == u2.getHp());
        assert(u1.getCurrentUnits() == u2.getCurrentUnits());
        assert(u1.getPosX() == u2.getPosX());
        assert(u1.getPosY() == u2.getPosY());
        assert(u1.getCooldownRemaining() == u2.getCooldownRemaining());
    }

    std::cout << "-> PASSED Muc 7: Cung seed chay hai lan khop tung bit!\n" << std::endl;
}

// =============================================================================
// MỤC 8: Sát thương đồng thời (Simultaneous Damage Resolution) & Bất biến ID (M9.11)
// =============================================================================
void testSimultaneousDamageResolution(const sim::SimConfig& config) {
    std::cout << "[TestCombat] [Muc 8] Kiem tra: Sat thuong dong thoi & bat bien thu tu ID..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    assert(cfgInf != nullptr);

    // 1. Thử nghiệm với PlayerFirst (chế độ cũ trước M9.10): Đơn vị ID thấp bắn trước gây thiệt hại
    {
        sim::WorldMap map(128, 96, 32);
        sim::WorldSim sim(map);
        sim.loadConfig();
        sim.setCombatOrderMode(sim::CombatOrderMode::PlayerFirst);

        // Đặt 2 đơn vị bộ binh cạnh nhau trong tầm bắn (5 tiles)
        // Unit 1 (Player, ID 1) tại (50, 50)
        // Unit 2 (Enemy, ID 2) tại (50, 51)
        sim.getUnits().emplace_back(1, sim::Faction::Player, sim::UnitType::Infantry, 50500, 50500, *cfgInf);
        sim.getUnits().emplace_back(2, sim::Faction::Enemy, sim::UnitType::Infantry, 50500, 51500, *cfgInf);

        sim.tick();

        const auto& u1 = sim.getUnits()[0];
        const auto& u2 = sim.getUnits()[1];

        // Ở PlayerFirst: u1 bắn trước làm giảm quân số u2, nên u2 phản công yếu hơn -> u1 HP > u2 HP
        std::cout << "  - PlayerFirst: Player HP=" << u1.getHp() << " (" << u1.getCurrentUnits() << " tr), Enemy HP="
                  << u2.getHp() << " (" << u2.getCurrentUnits() << " tr)" << std::endl;
        assert(u1.getHp() > u2.getHp());
    }

    // 2. Thử nghiệm với Simultaneous (M9.10+): Cả hai cùng gây sát thương đồng thời
    int hpAfterSim1_Player = 0;
    int hpAfterSim1_Enemy = 0;
    {
        sim::WorldMap map(128, 96, 32);
        sim::WorldSim sim(map);
        sim.loadConfig();
        sim.setCombatOrderMode(sim::CombatOrderMode::Simultaneous);

        // ID 1 Player, ID 2 Enemy
        sim.getUnits().emplace_back(1, sim::Faction::Player, sim::UnitType::Infantry, 50500, 50500, *cfgInf);
        sim.getUnits().emplace_back(2, sim::Faction::Enemy, sim::UnitType::Infantry, 50500, 51500, *cfgInf);

        sim.tick();

        const auto& u1 = sim.getUnits()[0];
        const auto& u2 = sim.getUnits()[1];

        std::cout << "  - Simultaneous (Player ID 1, Enemy ID 2): Player HP=" << u1.getHp() 
                  << ", Enemy HP=" << u2.getHp() << std::endl;
        assert(u1.getHp() == u2.getHp());
        assert(u1.getCurrentUnits() == u2.getCurrentUnits());
        hpAfterSim1_Player = u1.getHp();
        hpAfterSim1_Enemy = u2.getHp();
    }

    // 3. Đổi ngược thứ tự ID: Enemy ID 1, Player ID 2
    {
        sim::WorldMap map(128, 96, 32);
        sim::WorldSim sim(map);
        sim.loadConfig();
        sim.setCombatOrderMode(sim::CombatOrderMode::Simultaneous);

        // ID 1 Enemy, ID 2 Player
        sim.getUnits().emplace_back(1, sim::Faction::Enemy, sim::UnitType::Infantry, 50500, 51500, *cfgInf);
        sim.getUnits().emplace_back(2, sim::Faction::Player, sim::UnitType::Infantry, 50500, 50500, *cfgInf);

        sim.tick();

        const auto* uEnemy = sim.getUnitById(1);
        const auto* uPlayer = sim.getUnitById(2);
        assert(uEnemy != nullptr && uPlayer != nullptr);

        std::cout << "  - Simultaneous (Enemy ID 1, Player ID 2): Player HP=" << uPlayer->getHp() 
                  << ", Enemy HP=" << uEnemy->getHp() << std::endl;
        assert(uPlayer->getHp() == uEnemy->getHp());
        assert(uPlayer->getCurrentUnits() == uEnemy->getCurrentUnits());
        assert(uPlayer->getHp() == hpAfterSim1_Player);
        assert(uEnemy->getHp() == hpAfterSim1_Enemy);
    }

    std::cout << "-> PASSED Muc 8: Sat thuong dong thoi & bat bien thu tu ID!\n" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "KIEM THU TOAN DIEN HE THONG CHIEN DAU M5" << std::endl;
    std::cout << "==================================================\n" << std::endl;

    sim::SimConfig config;
    bool loaded = config.autoLoad();
    if (!loaded) {
        std::cerr << "LOI: Khong the nap cau hinh SimConfig tu data/game_config.json!" << std::endl;
        return 1;
    }

    testFormulaLocking(config);
    testShipTargetingOnlyShips(config);
    testShipBombardsShoreNotAir(config);
    testSplashDamageAndNoFriendlyFire(config);
    testCasualtiesReduceFutureFirepower(config);
    testCommanderAndEliminationVictoryConditions();
    testTimeoutVictoryConditions();
    testSimulationDeterminism();
    testSimultaneousDamageResolution(config);

    std::cout << "==================================================" << std::endl;
    std::cout << "TAT CA 8 MUC TEST CHIEN DAU M5/M9 DEU PASSED 100%!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
