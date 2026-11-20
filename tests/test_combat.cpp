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
// 1. Kiểm tra khóa công thức sát thương theo đúng bảng đã duyệt
// =============================================================================
void testFormulaLocking(const sim::SimConfig& config) {
    std::cout << "[TestCombat] 1. Kiem tra khoa cong thuc sat thuong theo bang duyet..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);
    const auto* cfgT14 = config.getUnitConfig(sim::UnitType::T14Armata);
    const auto* cfgSu = config.getUnitConfig(sim::UnitType::Su57);
    const auto* cfgMogami = config.getUnitConfig(sim::UnitType::Mogami);
    const auto* cfgAsagiri = config.getUnitConfig(sim::UnitType::Asagiri);

    assert(cfgInf && cfgLeo && cfgT14 && cfgSu && cfgMogami && cfgAsagiri);

    // 1.1. Bộ binh bắn bộ binh: 8.424 mỗi lượt
    int64_t dmgInfToInf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], cfgInf->unitsPerDot,
        cfgInf->armor, cfgInf->domain, false
    );
    std::cout << "  - Bo binh -> Bo binh: " << dmgInfToInf << " (ky vong 8424)" << std::endl;
    assert(dmgInfToInf == 8424);

    // 1.2. Bộ binh bắn Leopard 2A: 820 mỗi lượt
    int64_t dmgInfToLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], cfgInf->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, false
    );
    std::cout << "  - Bo binh -> Leopard 2A: " << dmgInfToLeo << " (ky vong 820)" << std::endl;
    assert(dmgInfToLeo == 820);

    // 1.3. Leopard 2A bắn bộ binh: tự chọn HE đạt 9.022 mỗi lượt
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

    // 1.4. T-14 Armata bắn Leopard 2A: tự chọn AP đạt 4.104 mỗi lượt
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

    // 1.5. Su-57 bắn Leopard 2A: 1.641 (kỳ vọng với accuracy 0.6)
    int64_t dmgSuToLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Su57, cfgSu->weapons[0], cfgSu->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, false
    );
    std::cout << "  - Su-57 -> Leopard 2A (AP, acc 0.6): " << dmgSuToLeo << " (ky vong 1641)" << std::endl;
    assert(dmgSuToLeo == 1641);

    // 1.6. Mogami bắn Asagiri chỉ bằng HE: 690 mỗi lượt
    // Tìm vũ khí HE trong Mogami
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

    std::cout << "[TestCombat] -> PASSED khoa cong thuc sat thuong!\n" << std::endl;
}

// =============================================================================
// 2. Quy tắc nhắm bắn (Targeting rules - Ai bắn được ai)
// =============================================================================
void testTargetingRules(const sim::SimConfig& config) {
    std::cout << "[TestCombat] 2. Kiem tra quy tac nham ban (Targeting rules)..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);
    const auto* cfgSu = config.getUnitConfig(sim::UnitType::Su57);
    const auto* cfgMogami = config.getUnitConfig(sim::UnitType::Mogami);
    const auto* cfgAsagiri = config.getUnitConfig(sim::UnitType::Asagiri);

    // Đặt vị trí các chấm cùng ô hoặc gần nhau để nằm trong tầm bắn
    sim::Unit infPlayer(1, sim::Faction::Player, sim::UnitType::Infantry, 10000, 10000, *cfgInf);
    sim::Unit infEnemy(2, sim::Faction::Enemy, sim::UnitType::Infantry, 11000, 10000, *cfgInf);
    sim::Unit leoEnemy(3, sim::Faction::Enemy, sim::UnitType::Leopard2A, 11000, 10000, *cfgLeo);
    sim::Unit suEnemy(4, sim::Faction::Enemy, sim::UnitType::Su57, 11000, 10000, *cfgSu);
    sim::Unit shipPlayer(5, sim::Faction::Player, sim::UnitType::Mogami, 10000, 10000, *cfgMogami);
    sim::Unit shipEnemy(6, sim::Faction::Enemy, sim::UnitType::Asagiri, 11000, 10000, *cfgAsagiri);

    // A. Không bắn đồng minh
    sim::Unit infPlayer2(7, sim::Faction::Player, sim::UnitType::Infantry, 11000, 10000, *cfgInf);
    assert(!sim::CombatSystem::canAttackTarget(infPlayer, infPlayer2));

    // B. Bộ binh bắn được bộ binh địch và xe tăng địch
    assert(sim::CombatSystem::canAttackTarget(infPlayer, infEnemy));
    assert(sim::CombatSystem::canAttackTarget(infPlayer, leoEnemy));

    // C. Bộ binh KHÔNG bắn được máy bay
    assert(!sim::CombatSystem::canAttackTarget(infPlayer, suEnemy));

    // D. KHÔNG ĐƠN VỊ NÀO ĐÁNH ĐƯỢC TÀU TRỪ TÀU (Quy tắc đã chốt)
    assert(!sim::CombatSystem::canAttackTarget(infPlayer, shipEnemy)); // Bộ binh không bắn được tàu
    sim::Unit suPlayer(8, sim::Faction::Player, sim::UnitType::Su57, 10000, 10000, *cfgSu);
    assert(!sim::CombatSystem::canAttackTarget(suPlayer, shipEnemy));  // Máy bay không bắn được tàu
    sim::Unit leoPlayer(9, sim::Faction::Player, sim::UnitType::Leopard2A, 10000, 10000, *cfgLeo);
    assert(!sim::CombatSystem::canAttackTarget(leoPlayer, shipEnemy)); // Xe tăng không bắn được tàu

    // E. Tàu bắn được tàu khác
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, shipEnemy));

    // F. Tàu bắn được vào bờ (mục tiêu Land)
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, infEnemy));
    assert(sim::CombatSystem::canAttackTarget(shipPlayer, leoEnemy));

    // G. Tàu không bắn được máy bay
    assert(!sim::CombatSystem::canAttackTarget(shipPlayer, suEnemy));

    // H. Khoảng cách ngoài tầm bắn
    sim::Unit farEnemy(10, sim::Faction::Enemy, sim::UnitType::Infantry, 50000, 50000, *cfgInf);
    assert(!sim::CombatSystem::canAttackTarget(infPlayer, farEnemy));

    std::cout << "[TestCombat] -> PASSED quy tac nham ban!\n" << std::endl;
}

// =============================================================================
// 3. Sát thương lan của đạn nổ (Splash damage)
// =============================================================================
void testSplashDamage(const sim::SimConfig& config) {
    std::cout << "[TestCombat] 3. Kiem tra sat thuong no lan (Splash damage)..." << std::endl;

    const auto* cfgCaesar = config.getUnitConfig(sim::UnitType::Caesar);
    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);
    const auto* cfgLeo = config.getUnitConfig(sim::UnitType::Leopard2A);

    // Caesar bắn đạn HE, splash = 1.5 ô
    assert(cfgCaesar->weapons[0].ammo == "HE");
    assert(cfgCaesar->weapons[0].splash >= 1.5f);

    // Sát thương chính lên Bộ binh
    int64_t mainDmgInf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Caesar, cfgCaesar->weapons[0], cfgCaesar->unitsPerDot,
        cfgInf->armor, cfgInf->domain, false
    );
    // Sát thương nổ lan lên Bộ binh đứng gần đó (55%)
    int64_t splashDmgInf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Caesar, cfgCaesar->weapons[0], cfgCaesar->unitsPerDot,
        cfgInf->armor, cfgInf->domain, true, -1, 550
    );
    std::cout << "  - Caesar HE main dmg: " << mainDmgInf << ", splash dmg to Infantry: " << splashDmgInf << std::endl;
    assert(splashDmgInf > 0 && splashDmgInf < mainDmgInf);
    // Tỉ lệ khoảng 55%
    double ratio = static_cast<double>(splashDmgInf) / mainDmgInf;
    assert(ratio >= 0.54 && ratio <= 0.56);

    // Sát thương nổ lan lên Leopard 2A đứng gần đó được tính theo giáp 68 của Leopard
    int64_t splashDmgLeo = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Caesar, cfgCaesar->weapons[0], cfgCaesar->unitsPerDot,
        cfgLeo->armor, cfgLeo->domain, true, -1, 550
    );
    std::cout << "  - Caesar HE splash dmg to Leopard (giap 68): " << splashDmgLeo << std::endl;
    // Do Leopard giáp dày 68, sát thương nổ lan phải nhỏ hơn nhiều so với lan vào bộ binh giáp 8
    assert(splashDmgLeo < splashDmgInf);

    std::cout << "[TestCombat] -> PASSED sat thuong no lan!\n" << std::endl;
}

// =============================================================================
// 4. Thương vong làm giảm số phát bắn (Casualty impact on firepower)
// =============================================================================
void testCasualtiesReduceFirepower(const sim::SimConfig& config) {
    std::cout << "[TestCombat] 4. Kiem tra thuong vong lam giam so phat ban..." << std::endl;

    const auto* cfgInf = config.getUnitConfig(sim::UnitType::Infantry);

    int fullUnits = 3000;
    int halfUnits = 1500;
    int oneUnit = 1;

    int64_t dmgFull = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], fullUnits,
        cfgInf->armor, cfgInf->domain, false
    );
    int64_t dmgHalf = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], halfUnits,
        cfgInf->armor, cfgInf->domain, false
    );
    int64_t dmgOne = sim::CombatSystem::calculateVolleyDamage(
        sim::UnitType::Infantry, cfgInf->weapons[0], oneUnit,
        cfgInf->armor, cfgInf->domain, false
    );

    std::cout << "  - 3000 linh: " << dmgFull << ", 1500 linh: " << dmgHalf << ", 1 linh: " << dmgOne << std::endl;
    assert(dmgFull == 8424);
    assert(dmgHalf == 4212); // Đúng một nửa
    assert(dmgOne >= 1);      // Có mức tối thiểu 1

    std::cout << "[TestCombat] -> PASSED thuong vong giam hoa luc!\n" << std::endl;
}

// =============================================================================
// 5. Điều kiện thắng thua và hết giờ (Victory / Defeat / Draw conditions)
// =============================================================================
void testVictoryConditions() {
    std::cout << "[TestCombat] 5. Kiem tra cac dieu kien thang thua va het gio..." << std::endl;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    bool loaded = sim.loadConfig();
    assert(loaded);

    // Khởi tạo 30 chấm
    sim.spawnDivisions(9999ULL);
    assert(!sim.isGameOver());

    // 5.1. Thua khi tư lệnh Player chết
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Player && u.getType() == sim::UnitType::Commander) {
            u.takeDamage(999999); // Tiêu diệt tư lệnh Player
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Defeat);
    std::cout << "  - Player Commander chet -> Defeat: OK" << std::endl;

    // 5.2. Thắng khi tư lệnh Enemy chết
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Enemy && u.getType() == sim::UnitType::Commander) {
            u.takeDamage(999999); // Tiêu diệt tư lệnh Enemy
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Victory);
    std::cout << "  - Enemy Commander chet -> Victory: OK" << std::endl;

    // 5.3. Thắng khi địch hết quân (chỉ còn tư lệnh)
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        if (u.getFaction() == sim::Faction::Enemy && u.getType() != sim::UnitType::Commander) {
            u.takeDamage(999999); // Tiêu diệt toàn bộ quân địch trừ Tư lệnh
        }
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Victory);
    std::cout << "  - Enemy het quan (chi con tu lenh) -> Victory: OK" << std::endl;

    // 5.4. Hòa khi cả 2 bên cùng hết quân (cả 2 tư lệnh cùng chết)
    sim.spawnDivisions(1234ULL);
    for (auto& u : sim.getUnits()) {
        u.takeDamage(999999); // Tiêu diệt tất cả
    }
    sim.checkMatchConditions();
    assert(sim.isGameOver());
    assert(sim.getMatchEndInfo().result == sim::MatchResult::Draw);
    std::cout << "  - Ca 2 ben deu chet het -> Draw: OK" << std::endl;

    std::cout << "[TestCombat] -> PASSED cac dieu kien thang thua!\n" << std::endl;
}

// =============================================================================
// 6. Tính tất định (Determinism check)
// =============================================================================
void testDeterminism() {
    std::cout << "[TestCombat] 6. Kiem tra tinh tat dinh (Determinism)..." << std::endl;

    sim::WorldMap map1(128, 96, 32);
    sim::WorldSim sim1(map1);
    sim1.loadConfig();

    sim::WorldMap map2(128, 96, 32);
    sim::WorldSim sim2(map2);
    sim2.loadConfig();

    uint64_t testSeed = 54321ULL;
    sim1.spawnDivisions(testSeed);
    sim2.spawnDivisions(testSeed);

    // Di chuyển một số đơn vị để chạm trán nhau
    std::vector<uint32_t> pIds = {sim1.getUnits()[1].getId(), sim1.getUnits()[11].getId()};
    sim1.queueMoveCommand(pIds, 64, 48);
    sim2.queueMoveCommand(pIds, 64, 48);

    // Chạy 50 ticks mô phỏng
    for (int t = 0; t < 50; ++t) {
        sim1.tick();
        sim2.tick();
    }

    // So sánh trạng thái từng đơn vị bit-for-bit
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

    std::cout << "[TestCombat] -> PASSED tinh tat dinh 100% bit-for-bit!\n" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "BAT DAU KIEM THU CHIEN DAU CO BAN (M5 COMBAT)" << std::endl;
    std::cout << "==================================================\n" << std::endl;

    sim::SimConfig config;
    bool loaded = config.autoLoad();
    if (!loaded) {
        std::cerr << "LOI: Khong the nap cau hinh SimConfig tu data/game_config.json!" << std::endl;
        return 1;
    }

    testFormulaLocking(config);
    testTargetingRules(config);
    testSplashDamage(config);
    testCasualtiesReduceFirepower(config);
    testVictoryConditions();
    testDeterminism();

    std::cout << "==================================================" << std::endl;
    std::cout << "TAT CA CAC TEST CHIEN DAU M5 DA VUOT QUA THANH CONG!" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
