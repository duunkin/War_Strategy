#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "sim/WorldSim.hpp"
#include "sim/vision/VisionSystem.hpp"
#include "sim/combat/CombatSystem.hpp"
#include "sim/config/SimConfig.hpp"
#include "sim/units/Unit.hpp"
#include "sim/map/WorldMap.hpp"

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>

using namespace sim;

void testVisionRadiusAndTerrainModifiers() {
    std::cout << "[TestVision] 1. Kiem tra ban kinh tam nhin va he so dia hinh (visionMod)..." << std::endl;

    SimConfig config;
    bool ok = config.autoLoad();
    assert(ok && "SimConfig autoLoad that bai!");

    const auto* infCfg = config.getUnitConfig(UnitType::Infantry);
    assert(infCfg != nullptr);
    assert(infCfg->visionTiles == 7);

    // Bán kính cơ sở: 7 ô
    // Rừng: visionMod = -1 -> rEff = 6
    // Đồi: visionMod = +2 -> rEff = 9
    // Núi: visionMod = +3 -> rEff = 10
    // Đồng bằng: visionMod = 0 -> rEff = 7
    int forestMod = config.getTerrainProperty(DetailedTerrainType::Forest).visionMod;
    int hillMod = config.getTerrainProperty(DetailedTerrainType::Hill).visionMod;
    int mountainMod = config.getTerrainProperty(DetailedTerrainType::Mountain).visionMod;
    int plainMod = config.getTerrainProperty(DetailedTerrainType::Plain).visionMod;

    assert(forestMod == -1);
    assert(hillMod == 2);
    assert(mountainMod == 3);
    assert(plainMod == 0);

    assert(std::max(2, infCfg->visionTiles + forestMod) == 6);
    assert(std::max(2, infCfg->visionTiles + hillMod) == 9);
    assert(std::max(2, infCfg->visionTiles + mountainMod) == 10);
    assert(std::max(2, infCfg->visionTiles + plainMod) == 7);

    const auto* su57Cfg = config.getUnitConfig(UnitType::Su57);
    assert(su57Cfg != nullptr && su57Cfg->visionTiles == 12);

    const auto* caesarCfg = config.getUnitConfig(UnitType::Caesar);
    assert(caesarCfg != nullptr && caesarCfg->visionTiles == 6);

    std::cout << "  -> PASSED: Ban kinh tam nhin va he so visionMod hoan toan chinh xac!" << std::endl;
}

void testLineOfSightElevationBlocking() {
    std::cout << "[TestVision] 2. Kiem tra chan duong nhin Line of Sight theo do cao..." << std::endl;

    // Bản đồ test 20x20
    std::vector<int> elevations(20 * 20, 0); // Toàn bộ đồng bằng (h=0)
    auto getElevation = [&](int x, int y) -> int {
        if (x < 0 || x >= 20 || y < 0 || y >= 20) return 0;
        return elevations[y * 20 + x];
    };

    // A. Đồi (h=2) nhìn đồi khác (h=2):
    // Điểm quan sát (5, 5), mục tiêu (10, 5)
    // 1) Giữa là đồng bằng (h=0) -> Nhìn thấy rõ
    bool losClear = VisionSystem::isLineOfSightClear(5, 5, 2, 10, 5, 2, getElevation, 4);
    assert(losClear && "Doi nhin doi qua vung dong bang o giua phai thay ro!");

    // 2) Giữa là rừng (h=1) tại (7, 5) -> Vẫn nhìn vượt qua rừng được (vì h_los = 2 > h_forest = 1)
    elevations[5 * 20 + 7] = 1;
    losClear = VisionSystem::isLineOfSightClear(5, 5, 2, 10, 5, 2, getElevation, 4);
    assert(losClear && "Doi nhin doi qua rung thap o giua phai thay ro!");

    // 3) Giữa là đồi khác (h=2) tại (7, 5) -> Bị chắn bởi cùng độ cao!
    elevations[5 * 20 + 7] = 2;
    losClear = VisionSystem::isLineOfSightClear(5, 5, 2, 10, 5, 2, getElevation, 4);
    assert(!losClear && "Doi nhin doi bi chan boi doi cung do cao o giua!");

    // 4) Giữa là núi (h=3) tại (7, 5) -> Bị chắn bởi núi cao hơn
    elevations[5 * 20 + 7] = 3;
    losClear = VisionSystem::isLineOfSightClear(5, 5, 2, 10, 5, 2, getElevation, 4);
    assert(!losClear && "Doi nhin doi bi chan boi nui cao hon o giua!");
    elevations[5 * 20 + 7] = 0; // Reset lại

    // B. Núi (h=3) tại (0, 0) nhìn xuống đồng bằng (h=0) tại (6, 0):
    // Đường dốc tại (2, 0): t = 2/6 = 1/3 -> h_los = 2.0
    // Tại (4, 0): t = 4/6 = 2/3 -> h_los = 1.0
    // Nếu tại (4, 0) là rừng (h=1): h_mid <= h_los -> không vượt quá đường dốc -> Nhìn thấy!
    elevations[0 * 20 + 4] = 1;
    losClear = VisionSystem::isLineOfSightClear(0, 0, 3, 6, 0, 0, getElevation, 4);
    assert(losClear && "Nui nhin xuong dong bang vuot qua rung khong vuot qua duong doc!");

    // Nếu tại (4, 0) là đồi (h=2): h_mid (2) > h_los (1.0) -> nhô quá đường dốc -> Bị chắn!
    elevations[0 * 20 + 4] = 2;
    losClear = VisionSystem::isLineOfSightClear(0, 0, 3, 6, 0, 0, getElevation, 4);
    assert(!losClear && "Nui nhin xuong dong bang bi chan boi doi nho qua duong doc!");
    elevations[0 * 20 + 4] = 0;

    // C. Rừng (h=1) nhìn xuyên rừng:
    // Quan sát tại (2, 2) [rừng h=1], mục tiêu (4, 2) [rừng h=1], ở giữa (3, 2) [rừng h=1]
    elevations[2 * 20 + 2] = 1;
    elevations[2 * 20 + 3] = 1;
    elevations[2 * 20 + 4] = 1;
    losClear = VisionSystem::isLineOfSightClear(2, 2, 1, 4, 2, 1, getElevation, 4);
    assert(!losClear && "Khong the nhin xuyen qua rung sang o rung khac o xa!");

    // D. Máy bay (h=4):
    // Máy bay quan sát mọi địa hình qua núi cao (h=3) -> luôn thấy
    elevations[5 * 20 + 7] = 3;
    losClear = VisionSystem::isLineOfSightClear(5, 5, 4, 10, 5, 0, getElevation, 4);
    assert(losClear && "May bay o do cao 4 luon nhin qua moi dia hinh chan!");

    // Mục tiêu là máy bay (h=4): quân mặt đất nhìn lên máy bay qua núi -> luôn thấy
    losClear = VisionSystem::isLineOfSightClear(5, 5, 0, 10, 5, 4, getElevation, 4);
    assert(losClear && "May bay dich luon bi lo, khong bi dia hinh che!");

    std::cout << "  -> PASSED: Quy tac chan tam nhin Line of Sight hoat dong hoan hao!" << std::endl;
}

void testExploredTilesMemory() {
    std::cout << "[TestVision] 3. Kiem tra o da kham pha duoc nho lai (Explored memory)..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    bool ok = sim.loadConfig();
    assert(ok);

    // Khởi tạo fog toàn bộ là Unexplored (0)
    std::vector<uint8_t> fog(128 * 96, static_cast<uint8_t>(FogState::Unexplored));

    const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
    assert(infCfg);

    // Đặt 1 tiểu đoàn bộ binh Player tại (10, 10)
    std::vector<Unit> units;
    units.emplace_back(1, Faction::Player, UnitType::Infantry, 10 * 1000 + 500, 10 * 1000 + 500, *infCfg);

    // Tick 1: Cập nhật tầm nhìn lần đầu
    VisionSystem::updateFactionVision(Faction::Player, units, map, sim.getConfig(), fog);

    // Ô (10, 10) và xung quanh phải ở trạng thái Visible (2)
    assert(fog[10 * 128 + 10] == static_cast<uint8_t>(FogState::Visible));
    assert(fog[10 * 128 + 14] == static_cast<uint8_t>(FogState::Visible)); // Trong bán kính 7
    // Ô xa như (50, 50) vẫn phải là Unexplored (0)
    assert(fog[50 * 128 + 50] == static_cast<uint8_t>(FogState::Unexplored));

    // Tick 2: Đơn vị di chuyển đi xa đến (50, 50)
    units[0].setPositionMilli(50 * 1000 + 500, 50 * 1000 + 500);
    VisionSystem::updateFactionVision(Faction::Player, units, map, sim.getConfig(), fog);

    // Vị trí cũ (10, 10) phải chuyển về Explored (1), KHÔNG ĐƯỢC quay về Unexplored (0)!
    assert(fog[10 * 128 + 10] == static_cast<uint8_t>(FogState::Explored) && "O da di qua phai chuyen ve Explored (1)!");
    assert(fog[10 * 128 + 14] == static_cast<uint8_t>(FogState::Explored) && "Vung cu phai la Explored (1)!");

    // Vị trí mới (50, 50) nay trở thành Visible (2)
    assert(fog[50 * 128 + 50] == static_cast<uint8_t>(FogState::Visible) && "O moi phai tro thanh Visible (2)!");

    // Vùng chưa bao giờ đến như (100, 80) vẫn là Unexplored (0)
    assert(fog[80 * 128 + 100] == static_cast<uint8_t>(FogState::Unexplored));

    std::cout << "  -> PASSED: Bo nho o da kham pha (Explored) duy tri chuan xac!" << std::endl;
}

void testCommanderStealthAndFiringReveal() {
    std::cout << "[TestVision] 4. Kiem tra tang hinh tu lenh va lo dien sau khi no sung..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    bool ok = sim.loadConfig();
    assert(ok);

    const auto* cmdCfg = sim.getConfig().getUnitConfig(UnitType::Commander);
    const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
    assert(cmdCfg && infCfg);

    // Tạo fog với khu vực trung tâm là Visible cho Player
    std::vector<uint8_t> playerFog(128 * 96, static_cast<uint8_t>(FogState::Unexplored));
    for (int y = 40; y <= 60; ++y) {
        for (int x = 40; x <= 60; ++x) {
            playerFog[y * 128 + x] = static_cast<uint8_t>(FogState::Visible);
        }
    }

    std::vector<Unit> units;
    // Tư lệnh Enemy tại (50, 50)
    units.emplace_back(1, Faction::Enemy, UnitType::Commander, 50 * 1000 + 500, 50 * 1000 + 500, *cmdCfg);

    // Bộ binh Player tại (45, 50) - cách 5 ô (> 1 ô stealthRevealRangeTiles)
    units.emplace_back(2, Faction::Player, UnitType::Infantry, 45 * 1000 + 500, 50 * 1000 + 500, *infCfg);

    // 1) Ô (50, 50) dù là Visible với Player nhưng tư lệnh địch ở xa > 1 ô -> TÀNG HÌNH!
    bool vis = VisionSystem::isUnitVisibleToFaction(units[0], Faction::Player, units, playerFog, 128, 96);
    assert(!vis && "Tu lenh dich cach xa > 1 o phai tang hinh truoc mat dich!");

    // 2) Bộ binh Player tiến lại gần trong vòng 1 ô (cụ thể: 49.8, 50 - cách 0.7 ô = 35m)
    units[1].setPositionMilli(49 * 1000 + 800, 50 * 1000 + 500);
    vis = VisionSystem::isUnitVisibleToFaction(units[0], Faction::Player, units, playerFog, 128, 96);
    assert(vis && "Tu lenh dich phai bi lo dien khi co quan dich ap sat trong vong 1 o (50m)!");

    // 3) Rút quân Player ra xa trở lại (cách 5 ô) -> tư lệnh ẩn lại
    units[1].setPositionMilli(45 * 1000 + 500, 50 * 1000 + 500);
    vis = VisionSystem::isUnitVisibleToFaction(units[0], Faction::Player, units, playerFog, 128, 96);
    assert(!vis);

    // 4) Tư lệnh địch nổ súng -> được đặt 40 ticks lộ diện
    units[0].setRevealedTicks(40);
    assert(units[0].isRevealed());

    // Vì ô (50, 50) đang là Visible đối với Player, tư lệnh bị lộ diện!
    vis = VisionSystem::isUnitVisibleToFaction(units[0], Faction::Player, units, playerFog, 128, 96);
    assert(vis && "Tu lenh phai lo dien sau khi no sung trong 40 ticks!");

    // Giảm dần ticks về 0
    for (int t = 0; t < 40; ++t) {
        units[0].decrementRevealedTicks();
    }
    assert(!units[0].isRevealed());

    // Sau 40 ticks, tư lệnh tàng hình trở lại!
    vis = VisionSystem::isUnitVisibleToFaction(units[0], Faction::Player, units, playerFog, 128, 96);
    assert(!vis && "Het 40 ticks, tu lenh phai tang hinh tro lai!");

    std::cout << "  -> PASSED: Co che tang hinh tu lenh va lo dien sau khi ban cuc ky chuan!" << std::endl;
}

void testTargetingRequiresVisionAndArtillerySpotter() {
    std::cout << "[TestVision] 5. Kiem tra muc tieu ngoai tam nhin khong bi ban & co che chi thi muc tieu (Spotter)..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    bool ok = sim.loadConfig();
    assert(ok);

    const auto* caesarCfg = sim.getConfig().getUnitConfig(UnitType::Caesar);
    const auto* leopardCfg = sim.getConfig().getUnitConfig(UnitType::Leopard2A);
    const auto* infCfg = sim.getConfig().getUnitConfig(UnitType::Infantry);
    const auto* cmdCfg = sim.getConfig().getUnitConfig(UnitType::Commander);
    assert(caesarCfg && leopardCfg && infCfg && cmdCfg);

    // Tình huống A: Pháo Caesar Player tại (20, 20) (tầm bắn 14, tầm nhìn 6)
    // Xe tăng Leopard Enemy tại (32, 20) (cách 12 ô -> trong tầm bắn 14, ngoài tầm nhìn 6 của Caesar)
    // Chưa có trinh sát: ô (32, 20) là Unexplored
    auto& units = sim.getUnits();
    units.clear();
    // Đặt 2 tư lệnh ở góc xa để trận đấu duy trì trạng thái InProgress
    units.emplace_back(100, Faction::Player, UnitType::Commander, 2 * 1000, 2 * 1000, *cmdCfg);
    units.emplace_back(101, Faction::Enemy, UnitType::Commander, 120 * 1000, 90 * 1000, *cmdCfg);
    units[0].setCooldownRemaining(100);
    units[1].setCooldownRemaining(100);

    units.emplace_back(1, Faction::Player, UnitType::Caesar, 20 * 1000 + 500, 20 * 1000 + 500, *caesarCfg);
    units.emplace_back(2, Faction::Enemy, UnitType::Leopard2A, 32 * 1000 + 500, 20 * 1000 + 500, *leopardCfg);

    int initialHp = sim.getUnitById(2)->getHp();

    // Thực hiện 1 tick mô phỏng
    sim.tick();

    // Caesar không thể nhìn thấy Leopard -> KHÔNG ĐƯỢC BẮN! Máu Leopard giữ nguyên!
    assert(sim.getUnitById(2)->getHp() == initialHp && "Muc tieu trong suong mu khong duoc phep bi ban!");

    // Tình huống B: Đưa 1 chấm Bộ binh Player (chỉ huy trinh sát) đứng tại (30, 20)
    // Bộ binh có tầm nhìn 7 ô -> nhìn thấy Leopard tại (32, 20)
    units.emplace_back(3, Faction::Player, UnitType::Infantry, 30 * 1000 + 500, 20 * 1000 + 500, *infCfg);
    // Đặt cooldown cho bộ binh để chỉ kiểm tra phát bắn của Caesar
    sim.getUnitById(3)->setCooldownRemaining(100);

    // Cho Caesar sẵn sàng bắn
    sim.getUnitById(1)->setCooldownRemaining(0);

    // Chạy tick tiếp theo
    sim.tick();

    // Caesar bắn trúng Leopard nhờ tầm nhìn chỉ thị của đồng đội (Spotter)!
    assert(sim.getUnitById(2)->getHp() < initialHp && "Caesar phai ban duoc muc tieu nho dong doi spotter nhin thay!");

    // Tình huống C: Sát thương nổ lan (splash) trúng cả Tư lệnh đang tàng hình
    // Đặt Tư lệnh Enemy tàng hình tại (33, 20) (cách Leopard 1 ô, nằm trong splash 1.5 của đạn HE)
    units.emplace_back(4, Faction::Enemy, UnitType::Commander, 33 * 1000 + 500, 20 * 1000 + 500, *cmdCfg);
    int cmdInitialHp = sim.getUnitById(4)->getHp();

    // Rút bộ binh ra xa (25, 20) để Tư lệnh Enemy vẫn tàng hình (cách bộ binh 8 ô)
    sim.getUnitById(3)->setPositionMilli(25 * 1000 + 500, 20 * 1000 + 500);

    // Cho Caesar bắn tiếp vào Leopard
    sim.getUnitById(1)->setCooldownRemaining(0);
    sim.tick();

    // Tư lệnh tàng hình đứng cạnh bị trúng đạn nổ lan (hoặc bị tiêu diệt hoàn toàn vì máu tư lệnh chỉ có 100)
    bool cmdTookSplashDamage = (sim.getUnitById(4) == nullptr) || (sim.getUnitById(4)->getHp() < cmdInitialHp);
    assert(cmdTookSplashDamage && "Sat thuong no lan phai trung ca tu lenh dang tang hinh!");

    std::cout << "  -> PASSED: Khong ban duoc muc tieu trong suong mu, Caesar ban theo spotter va splash trung tu lenh!" << std::endl;
}

void testVisionDeterminism() {
    std::cout << "[TestVision] 6. Kiem tra tinh tat dinh tuyet doi cua he thong tam nhin..." << std::endl;

    WorldMap map1(128, 96, 32);
    WorldSim sim1(map1);
    sim1.loadConfig();
    sim1.spawnDivisions(99998888ULL);

    WorldMap map2(128, 96, 32);
    WorldSim sim2(map2);
    sim2.loadConfig();
    sim2.spawnDivisions(99998888ULL);

    // Ra cùng 1 lệnh di chuyển cho cả 2 sim
    std::vector<uint32_t> moveIds = {1, 2, 3, 4, 5};
    sim1.queueMoveCommand(moveIds, 60, 48);
    sim2.queueMoveCommand(moveIds, 60, 48);

    // Chạy 50 ticks
    for (int t = 0; t < 50; ++t) {
        sim1.tick();
        sim2.tick();
    }

    // So sánh byte-for-byte m_fogPlayer và m_fogEnemy
    const auto& fog1P = sim1.getFogMap(Faction::Player);
    const auto& fog2P = sim2.getFogMap(Faction::Player);
    assert(fog1P.size() == fog2P.size());
    assert(std::memcmp(fog1P.data(), fog2P.data(), fog1P.size()) == 0 && "Fog Player hai sim phai giong het nhau tung byte!");

    const auto& fog1E = sim1.getFogMap(Faction::Enemy);
    const auto& fog2E = sim2.getFogMap(Faction::Enemy);
    assert(fog1E.size() == fog2E.size());
    assert(std::memcmp(fog1E.data(), fog2E.data(), fog1E.size()) == 0 && "Fog Enemy hai sim phai giong het nhau tung byte!");

    // Đo thời gian cập nhật vision
    double updateMs = sim1.getLastVisionUpdateTimeMs();
    std::cout << "  [Benchmark] Thoi gian cap nhat tam nhin thuc te tren may: " << updateMs << " ms / tick" << std::endl;
    assert(updateMs < 1.0 && "Thoi gian cap nhat tam nhin phai duoi 1.0 ms!");

    std::cout << "  -> PASSED: Tinh tat dinh va hieu nang he thong tam nhin dat chuan xuat sac!" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "KIEM THU TOAN DIEN HE THONG TAM NHIN & SUONG MU M6" << std::endl;
    std::cout << "==================================================" << std::endl;

    testVisionRadiusAndTerrainModifiers();
    testLineOfSightElevationBlocking();
    testExploredTilesMemory();
    testCommanderStealthAndFiringReveal();
    testTargetingRequiresVisionAndArtillerySpotter();
    testVisionDeterminism();

    std::cout << "==================================================" << std::endl;
    std::cout << "TAT CA 6 MUC TEST TAM NHIN M6 DEU PASSED 100%!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
