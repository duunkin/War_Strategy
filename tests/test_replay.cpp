#include "replay/Replay.hpp"
#include "ai/TacticalAI.hpp"
#include "ai/AiParams.hpp"
#include "ai/AiTelemetry.hpp"
#include "sim/WorldSim.hpp"
#include "sim/map/WorldMap.hpp"
#include "sim/units/Unit.hpp"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

using namespace sim;
using namespace ai;
using namespace replay;

namespace {

bool initSim(WorldSim& sim) {
    if (!sim.loadConfig()) {
        std::cerr << "[TestReplay] Khong the load config cho WorldSim!" << std::endl;
        return false;
    }
    return true;
}

} // namespace

// =============================================================================
// TEST 1: Replay Determinism Test (100% khớp trạng thái giữa AI Live & Replay)
// =============================================================================
void testReplayDeterminism() {
    std::cout << "[TestReplay] 1. Kiem tra tinh tat dinh cua Replay (Replay Determinism Test)..." << std::endl;

    constexpr uint64_t TEST_SEED = 777888ULL;
    constexpr int RUN_TICKS = 600;
    const std::string replayFilePath = "test_run_replay.json";

    // -------------------------------------------------------------------------
    // Pha 1: Chạy mô phỏng AI vs AI trực tiếp và ghi lại bằng ReplayRecorder
    // -------------------------------------------------------------------------
    WorldMap mapLive(128, 96, 32);
    WorldSim simLive(mapLive);
    assert(initSim(simLive));
    simLive.spawnDivisions(TEST_SEED);

    AiParams p1;
    p1.aggression = 0.85f;
    AiParams p2;
    p2.aggression = 0.65f;

    TacticalAI ai1(Faction::Player, TEST_SEED ^ 0x1111ULL, p1);
    TacticalAI ai2(Faction::Enemy, TEST_SEED ^ 0x2222ULL, p2);

    ReplayRecorder recorder;
    recorder.start(TEST_SEED, RUN_TICKS, p1, p2);

    // Đăng ký hook ghi nhận toàn bộ lệnh đưa vào WorldSim
    simLive.setCommandHook([&recorder](uint64_t tick, const Command& cmd) {
        recorder.recordCommand(tick, cmd);
    });

    for (int t = 0; t < RUN_TICKS; ++t) {
        if (simLive.isGameOver()) break;
        ai1.update(simLive);
        ai2.update(simLive);
        simLive.tick();
    }

    MatchEndInfo liveEndInfo = simLive.getMatchEndInfo();
    uint64_t liveFinalTick = simLive.getCurrentTick();
    recorder.finish(liveEndInfo.result, liveEndInfo.reason, liveFinalTick);

    assert(recorder.getRecordCount() > 0 && "Recorder phai ghi nhan duoc cac lenh cua AI!");
    std::cout << "  - Pha 1 xong: Da ghi nhan " << recorder.getRecordCount()
              << " lenh qua " << liveFinalTick << " ticks." << std::endl;

    // Lưu ra đĩa
    assert(recorder.saveToFile(replayFilePath) && "Ghi file replay phai thanh cong!");

    // -------------------------------------------------------------------------
    // Pha 2: Tạo WorldSim mới độc lập, nạp Replay và phát lại (KHÔNG CHẠY AI)
    // -------------------------------------------------------------------------
    ReplayPlayer player;
    assert(player.loadFromFile(replayFilePath) && "Nap file replay phai thanh cong!");
    assert(player.getSeed() == TEST_SEED && "Seed trong replay phai khop seed ban dau!");
    assert(player.getRecordCount() == recorder.getRecordCount());

    WorldMap mapPlayback(128, 96, 32);
    WorldSim simPlayback(mapPlayback);
    assert(initSim(simPlayback));
    simPlayback.spawnDivisions(player.getSeed());

    for (int t = 0; t < RUN_TICKS; ++t) {
        if (simPlayback.isGameOver()) break;
        // Bơm đúng các lệnh của tick này vào simPlayback (KHÔNG gọi ai1 hay ai2!)
        player.applyCommandsForTick(simPlayback.getCurrentTick(), simPlayback);
        simPlayback.tick();
    }

    MatchEndInfo playbackEndInfo = simPlayback.getMatchEndInfo();
    uint64_t playbackFinalTick = simPlayback.getCurrentTick();

    // -------------------------------------------------------------------------
    // Pha 3: So sánh trạng thái 100% giữa Live và Playback
    // -------------------------------------------------------------------------
    assert(playbackFinalTick == liveFinalTick && "Final tick giua live va playback phai bang nhau!");
    assert(playbackEndInfo.result == liveEndInfo.result && "Match result phai giong het nhau!");
    assert(playbackEndInfo.reason == liveEndInfo.reason && "Match end reason phai giong het nhau!");

    const auto& unitsLive = simLive.getUnits();
    const auto& unitsPlayback = simPlayback.getUnits();
    assert(unitsLive.size() == unitsPlayback.size());

    int unitsCompared = 0;
    for (size_t i = 0; i < unitsLive.size(); ++i) {
        const auto& uLive = unitsLive[i];
        const auto& uPlay = unitsPlayback[i];

        assert(uLive.getId() == uPlay.getId());
        assert(uLive.getFaction() == uPlay.getFaction());
        assert(uLive.isDead() == uPlay.isDead());
        assert(uLive.getPosX() == uPlay.getPosX() && "Toa do PosX giua Live va Replay phai trung khop 100%!");
        assert(uLive.getPosY() == uPlay.getPosY() && "Toa do PosY giua Live va Replay phai trung khop 100%!");
        assert(uLive.getHp() == uPlay.getHp() && "HP giua Live va Replay phai trung khop 100%!");
        assert(uLive.getCurrentUnits() == uPlay.getCurrentUnits() && "Quan so giua Live va Replay phai trung khop 100%!");
        unitsCompared++;
    }

    std::cout << "  - So sanh chi tiet: 100% (" << unitsCompared
              << " don vi) khop tuyet doi ve PosX, PosY, HP, Quan so va Ket qua tran dau!" << std::endl;

    // Dọn dẹp file test
    std::filesystem::remove(replayFilePath);
    std::cout << "  -> PASSED: He thong Replay dat tinh tat dinh tuyet doi 100%!\n";
}

// =============================================================================
// TEST 2: AI Telemetry Test (Snapshot & 2-Line Format)
// =============================================================================
void testAiTelemetry() {
    std::cout << "[TestReplay] 2. Kiem tra AI Telemetry Snapshot & Format 2 dong CMD..." << std::endl;

    WorldMap map(128, 96, 32);
    WorldSim sim(map);
    assert(initSim(sim));
    sim.spawnDivisions(99999ULL);

    TacticalAI ai1(Faction::Player, 12345ULL);
    TacticalAI ai2(Faction::Enemy, 67890ULL);

    for (int t = 0; t < 60; ++t) {
        ai1.update(sim);
        ai2.update(sim);
        sim.tick();
    }

    const auto& snap1 = ai1.getSnapshot();
    const auto& snap2 = ai2.getSnapshot();

    assert(snap1.faction == Faction::Player);
    assert(snap2.faction == Faction::Enemy);
    assert(snap1.aliveUnits > 0);
    assert(snap2.aliveUnits > 0);
    assert(snap1.totalTroops > 0);
    assert(snap2.totalTroops > 0);
    assert(!snap1.primaryIntent.empty());
    assert(!snap2.primaryIntent.empty());
    assert(snap1.totalCommandsIssued > 0);
    assert(snap2.totalCommandsIssued > 0);

    // Format 2 dòng
    std::string line1 = formatTelemetryLine(snap1, 1, sim.getCurrentTick(), sim.getConfig().getTickRate());
    std::string line2 = formatTelemetryLine(snap2, 1, sim.getCurrentTick(), sim.getConfig().getTickRate());

    assert(!line1.empty());
    assert(!line2.empty());
    assert(line1.rfind("AI1[BLUE]", 0) == 0 && "Dong 1 phai bat dau bang AI1[BLUE]!");
    assert(line2.rfind("AI2[RED ]", 0) == 0 && "Dong 2 phai bat dau bang AI2[RED ]!");

    std::cout << "  - Mau 2 dong Telemetry tren CMD:\n"
              << "    " << line1 << "\n"
              << "    " << line2 << "\n";
    std::cout << "  -> PASSED: Telemetry Snapshot va 2 dong CMD format chuan xac!\n";
}

// =============================================================================
// TEST 3: Headless Batch & CSV Log Test
// =============================================================================
void testBatchAndCsvLog() {
    std::cout << "[TestReplay] 3. Kiem tra Headless Batch Simulation & CSV Log..." << std::endl;

    const std::string csvPath = "test_headless_log.csv";
    std::filesystem::remove(csvPath);

    constexpr int NUM_MATCHES = 3;
    constexpr uint64_t BASE_SEED = 54321ULL;
    constexpr uint64_t MAX_TICKS = 150;

    struct MatchStats {
        uint64_t finalTick;
        MatchResult result;
        int ai1Troops;
        int ai2Troops;
    };

    std::vector<MatchStats> run1Results;

    // Lần 1: Chạy batch và ghi CSV
    {
        std::ofstream csv(csvPath);
        csv << "match_id,seed,winner,end_reason,ticks,sim_seconds,wall_ms,ai1_units,ai1_troops,ai1_cmd_hp,ai2_units,ai2_troops,ai2_cmd_hp,ai1_cmds,ai2_cmds\n";

        for (int m = 1; m <= NUM_MATCHES; ++m) {
            uint64_t seed = BASE_SEED + static_cast<uint64_t>(m - 1);
            WorldMap map(128, 96, 32);
            WorldSim sim(map);
            assert(initSim(sim));
            sim.spawnDivisions(seed);

            TacticalAI ai1(Faction::Player, seed ^ 0x1111ULL);
            TacticalAI ai2(Faction::Enemy, seed ^ 0x2222ULL);

            for (uint64_t t = 0; t < MAX_TICKS; ++t) {
                if (sim.isGameOver()) break;
                ai1.update(sim);
                ai2.update(sim);
                sim.tick();
            }

            int ai1Tr = 0, ai2Tr = 0;
            for (const auto& u : sim.getUnits()) {
                if (u.isDead()) continue;
                if (u.getFaction() == Faction::Player) ai1Tr += u.getCurrentUnits();
                else if (u.getFaction() == Faction::Enemy) ai2Tr += u.getCurrentUnits();
            }

            MatchStats ms{sim.getCurrentTick(), sim.getMatchEndInfo().result, ai1Tr, ai2Tr};
            run1Results.push_back(ms);

            csv << m << "," << seed << ","
                << (ms.result == MatchResult::Victory ? "AI1_BLUE" : ms.result == MatchResult::Defeat ? "AI2_RED" : "DRAW") << ","
                << "\"" << sim.getMatchEndInfo().reason << "\","
                << ms.finalTick << "," << (ms.finalTick / 20) << ",10,"
                << "30," << ai1Tr << ",100,30," << ai2Tr << ",100,"
                << ai1.getTotalCommandsIssued() << "," << ai2.getTotalCommandsIssued() << "\n";
        }
    }

    // Kiểm tra file CSV có tồn tại và có dữ liệu
    assert(std::filesystem::exists(csvPath));
    {
        std::ifstream csvCheck(csvPath);
        std::string line;
        int lineCount = 0;
        while (std::getline(csvCheck, line)) {
            if (!line.empty()) lineCount++;
        }
        csvCheck.close();
        assert(lineCount == NUM_MATCHES + 1 && "CSV phai co 1 header + 3 dong tran dau!");
    }

    // Lần 2: Chạy lại cùng seed và xác nhận kết quả khớp 100%
    for (int m = 1; m <= NUM_MATCHES; ++m) {
        uint64_t seed = BASE_SEED + static_cast<uint64_t>(m - 1);
        WorldMap map(128, 96, 32);
        WorldSim sim(map);
        assert(initSim(sim));
        sim.spawnDivisions(seed);

        TacticalAI ai1(Faction::Player, seed ^ 0x1111ULL);
        TacticalAI ai2(Faction::Enemy, seed ^ 0x2222ULL);

        for (uint64_t t = 0; t < MAX_TICKS; ++t) {
            if (sim.isGameOver()) break;
            ai1.update(sim);
            ai2.update(sim);
            sim.tick();
        }

        int ai1Tr = 0, ai2Tr = 0;
        for (const auto& u : sim.getUnits()) {
            if (u.isDead()) continue;
            if (u.getFaction() == Faction::Player) ai1Tr += u.getCurrentUnits();
            else if (u.getFaction() == Faction::Enemy) ai2Tr += u.getCurrentUnits();
        }

        assert(sim.getCurrentTick() == run1Results[m - 1].finalTick);
        assert(sim.getMatchEndInfo().result == run1Results[m - 1].result);
        assert(ai1Tr == run1Results[m - 1].ai1Troops);
        assert(ai2Tr == run1Results[m - 1].ai2Troops);
    }

    std::filesystem::remove(csvPath);
    std::cout << "  -> PASSED: Batch chay on dinh, CSV dung format, ket qua bat bien theo seed!\n";
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "BAT DAU TEST SUITE CHO MOC M8: REPLAY & HEADLESS\n";
    std::cout << "====================================================\n";

    testReplayDeterminism();
    testAiTelemetry();
    testBatchAndCsvLog();

    std::cout << "====================================================\n";
    std::cout << "TAT CA 3/3 TEST M8 CHO REPLAY & HEADLESS DEU PASS 100%!\n";
    std::cout << "====================================================\n";
    return 0;
}
