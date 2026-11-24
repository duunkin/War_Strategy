#include "../sim/WorldSim.hpp"
#include "../sim/map/WorldMap.hpp"
#include "../mapgen/MapGenerator.hpp"
#include "../mapgen/MapGenConfig.hpp"
#include "../ai/TacticalAI.hpp"
#include "../ai/AiParams.hpp"
#include "../ai/AiTelemetry.hpp"
#include "../replay/Replay.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <cstdint>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

struct HeadlessOptions {
    int         matches{1};
    uint64_t    seed{12345ULL};
    uint64_t    maxTicks{72000ULL};
    std::string ai1ParamsFile;
    std::string ai2ParamsFile;
    float       ai1AggOverride{-1.0f};
    float       ai2AggOverride{-1.0f};
    int         printEvery{20};
    int         traceEvery{0};
    bool        liveOutput{true};
    bool        scrollOutput{false};
    bool        quietOutput{false};
    std::string logCsvFile;
    std::string saveReplayFile;
    std::string replayDir;
    sim::CombatOrderMode combatOrderMode{sim::CombatOrderMode::Simultaneous};
    bool        mapFlipX{false};
};

void printHelp(const char* progName) {
    std::cout << "========================================================================\n"
              << "War Strategy - Headless AI vs AI Runner (M8)\n"
              << "========================================================================\n"
              << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --matches <N>          So tran AI vs AI can chay lien tiep (Mac dinh: 1)\n"
              << "  --seed <uint64>        Seed bat dau (tran i dung seed + i, mac dinh: 12345)\n"
              << "  --max-ticks <N>        Gioi han tick toi da moi tran (Mac dinh: 72000 = 60 phut)\n"
              << "  --ai1-params <file>    Nap file JSON tham so AiParams cho AI 1 (Blue)\n"
              << "  --ai2-params <file>    Nap file JSON tham so AiParams cho AI 2 (Red)\n"
              << "  --ai1-agg <val>        Ghi de nhanh do hung hang (aggression 0.0..1.0) AI 1\n"
              << "  --ai2-agg <val>        Ghi de nhanh do hung hang (aggression 0.0..1.0) AI 2\n"
              << "  --print-every <ticks>  Chu ky in 2 dong suy nghi AI len CMD (Mac dinh: 20; 0: tat)\n"
              << "  --trace-every <ticks>  Chu ky in chi tiet trang thai don vi va pathfinder len CMD (0: tat)\n"
              << "  --live                 Che do cap nhat tai cho 2 dong CMD qua ANSI (Mac dinh khi 1 tran)\n"
              << "  --scroll               Che do in cuon tung cap 2 dong kem duong ngan cach\n"
              << "  --quiet                Che do tat in live, chi in tien do de dat toc do toi da\n"
              << "  --log <file.csv>       Ghi log CSV tong hop ket qua tung tran\n"
              << "  --save-replay <file>   Luu replay tran dau vao file chi dinh (neu chay 1 tran)\n"
              << "  --replay-dir <dir>     Luu replay tat ca cac tran vao thu muc\n"
              << "  --help, -h             Hien thi tro giup nay\n"
              << "========================================================================\n";
}

bool enableWindowsAnsi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
#else
    return true;
#endif
}

bool loadAiParamsFromFile(const std::string& path, ai::AiParams& outParams) {
    if (!ai::loadTrainedAiProfile(path, outParams)) {
        std::cerr << "[Warning] Khong the mo hoac parse file AiParams: " << path << ", dung gia tri mac dinh.\n";
        return false;
    }
    return true;
}

std::string matchResultToWinnerString(sim::MatchResult res) {
    switch (res) {
        case sim::MatchResult::Victory: return "AI1_BLUE";
        case sim::MatchResult::Defeat:  return "AI2_RED";
        case sim::MatchResult::Draw:    return "DRAW";
        default:                        return "IN_PROGRESS";
    }
}

void printUnitTrace(const sim::WorldSim& sim, const ai::TacticalAI& ai1, const ai::TacticalAI& ai2, uint64_t tick) {
    std::cout << "\n========================================================================================================================================\n"
              << "[TRACE TICK " << tick << " (" << (tick / 20) << "s)] Living Units: " << sim.getUnits().size() << "\n"
              << "AI1 (Blue) Memory: " << ai1.getMemory().size() << " enemies | AI2 (Red) Memory: " << ai2.getMemory().size() << " enemies\n"
              << "----------------------------------------------------------------------------------------------------------------------------------------\n"
              << std::left << std::setw(4)  << "ID"
              << std::setw(10) << "Faction"
              << std::setw(12) << "Type"
              << std::setw(11) << "Troops"
              << std::setw(13) << "HP(%)"
              << std::setw(10) << "Pos(X,Y)"
              << std::setw(9)  << "HasDest"
              << std::setw(20) << "Dest->Final"
              << std::setw(10) << "NextStep"
              << std::setw(16) << "Pathfinder"
              << "AI Intent" << "\n"
              << "----------------------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& u : sim.getUnits()) {
        if (u.isDead()) continue;
        std::string factionStr = (u.getFaction() == sim::Faction::Player) ? "AI1_Blue" : "AI2_Red";
        std::string typeStr = u.getName();
        std::string troopsStr = std::to_string(u.getCurrentUnits()) + "/" + std::to_string(u.getMaxUnits());
        int hpPct = static_cast<int>(std::round(static_cast<float>(u.getHp()) * 100.0f / static_cast<float>(std::max(1, u.getMaxHp()))));
        std::string hpStr = std::to_string(u.getHp()) + " (" + std::to_string(hpPct) + "%)";
        std::string posStr = "(" + std::to_string(u.getTileX()) + "," + std::to_string(u.getTileY()) + ")[" + std::to_string(u.getPosX()) + "," + std::to_string(u.getPosY()) + "]";
        std::string hasDestStr = u.hasDestination() ? "TRUE" : "FALSE";

        std::string destStr = "None";
        if (u.hasDestination()) {
            destStr = "(" + std::to_string(u.getDestX() / 1000) + "," + std::to_string(u.getDestY() / 1000) + ")[" + std::to_string(u.getDestX()) + "," + std::to_string(u.getDestY()) + "]";
            if (u.hasPath()) {
                destStr += "->(" + std::to_string(u.getFinalTargetTile().x) + "," + std::to_string(u.getFinalTargetTile().y) + ")";
            }
        }

        std::string nextStepStr = "None";
        if (u.hasPath() && u.getCurrentWaypointIndex() < u.getWaypoints().size()) {
            const auto& wp = u.getWaypoints()[u.getCurrentWaypointIndex()];
            nextStepStr = "(" + std::to_string(wp.x) + "," + std::to_string(wp.y) + ")";
        }

        sim::DetailedTerrainType dt = sim.getDetailedTerrain(u.getTileX(), u.getTileY());
        int spdPct = sim.getConfig().getSpeedMultiplierPercent(dt, u.getDomain(), u.getType());

        std::string pathfinderStr = "N/A";
        if (u.wasPathfinderCalled()) {
            if (u.getLastPathfinderSuccess()) {
                pathfinderStr = "OK(len=" + std::to_string(u.getLastPathLength()) + ")";
            } else {
                pathfinderStr = "FAIL";
            }
        }
        pathfinderStr += "[dt=" + std::to_string(static_cast<int>(dt)) + ",spd=" + std::to_string(spdPct) + ",acc=" + std::to_string(u.getMoveAccumulator()) + "]";

        std::string intent = (u.getFaction() == sim::Faction::Player) ? ai1.getUnitIntent(u.getId()) : ai2.getUnitIntent(u.getId());

        std::cout << std::left << std::setw(4)  << u.getId()
                  << std::setw(10) << factionStr
                  << std::setw(12) << typeStr
                  << std::setw(11) << troopsStr
                  << std::setw(13) << hpStr
                  << std::setw(10) << posStr
                  << std::setw(9)  << hasDestStr
                  << std::setw(20) << destStr
                  << std::setw(10) << nextStepStr
                  << std::setw(16) << pathfinderStr
                  << intent << "\n";
    }
    std::cout << "========================================================================================================================================\n\n" << std::flush;
}

} // namespace

int main(int argc, char* argv[]) {
    HeadlessOptions opt;
    bool explicitDisplayMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--matches" && i + 1 < argc) {
            opt.matches = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            opt.seed = std::stoull(argv[++i]);
        } else if (arg == "--max-ticks" && i + 1 < argc) {
            opt.maxTicks = std::stoull(argv[++i]);
        } else if (arg == "--ai1-params" && i + 1 < argc) {
            opt.ai1ParamsFile = argv[++i];
        } else if (arg == "--ai2-params" && i + 1 < argc) {
            opt.ai2ParamsFile = argv[++i];
        } else if (arg == "--ai1-agg" && i + 1 < argc) {
            opt.ai1AggOverride = std::stof(argv[++i]);
        } else if (arg == "--ai2-agg" && i + 1 < argc) {
            opt.ai2AggOverride = std::stof(argv[++i]);
        } else if (arg == "--print-every" && i + 1 < argc) {
            opt.printEvery = std::max(0, std::stoi(argv[++i]));
        } else if (arg == "--trace-every" && i + 1 < argc) {
            opt.traceEvery = std::max(0, std::stoi(argv[++i]));
        } else if (arg == "--live") {
            opt.liveOutput = true;
            opt.scrollOutput = false;
            opt.quietOutput = false;
            explicitDisplayMode = true;
        } else if (arg == "--scroll") {
            opt.scrollOutput = true;
            opt.liveOutput = false;
            opt.quietOutput = false;
            explicitDisplayMode = true;
        } else if (arg == "--quiet") {
            opt.quietOutput = true;
            opt.liveOutput = false;
            opt.scrollOutput = false;
            explicitDisplayMode = true;
        } else if (arg == "--log" && i + 1 < argc) {
            opt.logCsvFile = argv[++i];
        } else if (arg == "--save-replay" && i + 1 < argc) {
            opt.saveReplayFile = argv[++i];
        } else if (arg == "--replay-dir" && i + 1 < argc) {
            opt.replayDir = argv[++i];
        } else if (arg == "--order-player-first") {
            opt.combatOrderMode = sim::CombatOrderMode::PlayerFirst;
        } else if (arg == "--order-enemy-first") {
            opt.combatOrderMode = sim::CombatOrderMode::EnemyFirst;
        } else if (arg == "--order-alternate") {
            opt.combatOrderMode = sim::CombatOrderMode::AlternateTicks;
        } else if (arg == "--order-simultaneous") {
            opt.combatOrderMode = sim::CombatOrderMode::Simultaneous;
        } else if (arg == "--map-flip-x") {
            opt.mapFlipX = true;
        } else if (arg == "--ablate-no-safe-retreat") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableSafeRetreatExit = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-recon-inherit") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableReconInheritance = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-deep-push") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableDeepPush = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-target-filter") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableTargetFiltering = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-cmd-stab") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableCommanderStabilization = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-gepard-escort") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableGepardFrontlineEscort = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-symmetry") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableSymmetryMirroring = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-deep-push-hp") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableDeepPushHpCheck = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-no-threat-radius") {
            auto flags = ai::TacticalAI::getGlobalAblationFlags();
            flags.enableThreatRadiusTiles = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        } else if (arg == "--ablate-all-off") {
            ai::TacticalAblationFlags flags;
            flags.enableSafeRetreatExit = false;
            flags.enableReconInheritance = false;
            flags.enableDeepPush = false;
            flags.enableTargetFiltering = false;
            flags.enableCommanderStabilization = false;
            flags.enableGepardFrontlineEscort = false;
            flags.enableSymmetryMirroring = false;
            flags.enableDeepPushHpCheck = false;
            flags.enableThreatRadiusTiles = false;
            ai::TacticalAI::setGlobalAblationFlags(flags);
        }
    }

    // Tự động điều chỉnh chế độ hiển thị nếu không chỉ định rõ
    bool isTty = (isatty(fileno(stdout)) != 0);
    if (!explicitDisplayMode) {
        if (opt.traceEvery > 0) {
            opt.scrollOutput = true;
            opt.liveOutput = false;
        } else if (!isTty) {
            opt.scrollOutput = true;
            opt.liveOutput = false;
        } else if (opt.matches > 1) {
            // Khi chạy nhiều trận: mặc định cuộn hoặc tóm tắt
            opt.scrollOutput = true;
            opt.liveOutput = false;
        } else {
            opt.liveOutput = true;
        }
    }

    if (opt.liveOutput) {
        if (!enableWindowsAnsi()) {
            opt.liveOutput = false;
            opt.scrollOutput = true;
        }
    }

    // Nạp tham số AI
    ai::AiParams baseAi1Params;
    ai::AiParams baseAi2Params;
    if (!opt.ai1ParamsFile.empty()) loadAiParamsFromFile(opt.ai1ParamsFile, baseAi1Params);
    if (!opt.ai2ParamsFile.empty()) loadAiParamsFromFile(opt.ai2ParamsFile, baseAi2Params);
    if (opt.ai1AggOverride >= 0.0f) baseAi1Params.aggression = opt.ai1AggOverride;
    if (opt.ai2AggOverride >= 0.0f) baseAi2Params.aggression = opt.ai2AggOverride;

    // Chuẩn bị CSV nếu có yêu cầu
    std::ofstream csvStream;
    if (!opt.logCsvFile.empty()) {
        std::filesystem::path p(opt.logCsvFile);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        bool writeHeader = !std::filesystem::exists(p) || (std::filesystem::file_size(p) == 0);
        csvStream.open(opt.logCsvFile, std::ios::out | std::ios::app);
        if (writeHeader && csvStream.is_open()) {
            csvStream << "match_id,seed,winner,end_reason,ticks,sim_seconds,wall_ms,"
                     << "ai1_units,ai1_troops,ai1_cmd_hp,ai2_units,ai2_troops,ai2_cmd_hp,ai1_cmds,ai2_cmds\n";
        }
    }

    std::cout << "========================================================================\n"
              << "WAR STRATEGY HEADLESS RUNNER\n"
              << "Matches: " << opt.matches << " | Base Seed: " << opt.seed
              << " | Max Ticks: " << opt.maxTicks << " (" << (opt.maxTicks / 20) << "s in-game)\n"
              << "AI1 (Blue) Agg: " << std::fixed << std::setprecision(2) << baseAi1Params.aggression
              << " | AI2 (Red) Agg: " << baseAi2Params.aggression << "\n"
              << "Display: " << (opt.liveOutput ? "LIVE (ANSI in-place)" : opt.scrollOutput ? "SCROLL" : "QUIET")
              << " | Print Every: " << opt.printEvery << " ticks\n"
              << "========================================================================\n\n";

    // Thống kê toàn bộ đợt chạy
    int ai1Wins = 0;
    int ai2Wins = 0;
    int draws = 0;
    int reasonKillCommander = 0;
    int reasonOutOfTroops = 0;
    int reasonTimeout = 0;
    uint64_t totalSimTicks = 0;
    auto batchStartTime = std::chrono::steady_clock::now();

    for (int matchIdx = 1; matchIdx <= opt.matches; ++matchIdx) {
        uint64_t currentSeed = opt.seed + static_cast<uint64_t>(matchIdx - 1);
        auto matchStartTime = std::chrono::steady_clock::now();

        sim::WorldMap map(128, 96, 32);
        sim::WorldSim sim(map);
        if (!sim.loadConfig()) {
            std::cerr << "[Error] Khong the load config cho tran #" << matchIdx << "!\n";
            continue;
        }

        mapgen::MapGenConfig mapCfg;
        mapgen::MapGenerator mapGen;
        mapGen.generate(map, currentSeed, mapCfg);
        if (opt.mapFlipX) {
            for (int y = 0; y < map.getHeight(); ++y) {
                for (int x = 0; x < map.getWidth() / 2; ++x) {
                    int oppX = map.getWidth() - 1 - x;
                    auto t1 = map.getTile(x, y);
                    auto t2 = map.getTile(oppX, y);
                    map.setTile(x, y, t2);
                    map.setTile(oppX, y, t1);
                }
            }
        }
        std::cout << "MAP: generated (seed=" << currentSeed << (opt.mapFlipX ? ", flipped_x" : "") << ")\n";

        sim.setCombatOrderMode(opt.combatOrderMode);
        sim.spawnDivisions(currentSeed);

        // Khởi tạo 2 AI độc lập hoàn toàn
        ai::TacticalAI ai1(sim::Faction::Player, currentSeed ^ 0x1111ULL, baseAi1Params);
        ai::TacticalAI ai2(sim::Faction::Enemy, currentSeed ^ 0x2222ULL, baseAi2Params);

        // Khởi tạo ReplayRecorder nếu cần
        replay::ReplayRecorder recorder;
        bool shouldRecord = (!opt.saveReplayFile.empty() && opt.matches == 1) || (!opt.replayDir.empty());
        if (shouldRecord) {
            recorder.start(currentSeed, opt.maxTicks, baseAi1Params, baseAi2Params);
            sim.setCommandHook([&recorder](uint64_t tick, const sim::Command& cmd) {
                recorder.recordCommand(tick, cmd);
            });
        }

        bool livePrintedFirstTime = false;
        auto lastConsolePrintTime = std::chrono::steady_clock::now();

        // Vòng lặp mô phỏng chính cho trận đấu
        while (!sim.isGameOver() && sim.getCurrentTick() < opt.maxTicks) {
            uint64_t tick = sim.getCurrentTick();

            // AI tính toán và gửi lệnh vào WorldSim
            bool enemyAiFirst = (opt.combatOrderMode == sim::CombatOrderMode::EnemyFirst) ||
                                (opt.combatOrderMode == sim::CombatOrderMode::AlternateTicks && tick % 2 == 1);
            if (enemyAiFirst) {
                ai2.update(sim);
                ai1.update(sim);
            } else {
                ai1.update(sim);
                ai2.update(sim);
            }

            // In telemetry 2 dòng nếu đủ điều kiện
            if (opt.printEvery > 0 && (tick % static_cast<uint64_t>(opt.printEvery) == 0)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastConsolePrintTime).count();

                // Giới hạn tần suất refresh console tối đa ~20fps (50ms) để không nghẽn CPU
                if (elapsedMs >= 40 || !opt.liveOutput) {
                    lastConsolePrintTime = now;
                    std::string line1 = ai::formatTelemetryLine(ai1.getSnapshot(), matchIdx, tick, sim.getConfig().getTickRate());
                    std::string line2 = ai::formatTelemetryLine(ai2.getSnapshot(), matchIdx, tick, sim.getConfig().getTickRate());

                    if (opt.liveOutput) {
                        if (livePrintedFirstTime) {
                            std::cout << "\033[2A\r" << line1 << "\033[K\n" << line2 << "\033[K\n" << std::flush;
                        } else {
                            std::cout << line1 << "\n" << line2 << "\n" << std::flush;
                            livePrintedFirstTime = true;
                        }
                    } else if (opt.scrollOutput) {
                        std::cout << line1 << "\n" << line2 << "\n"
                                  << "----------------------------------------------------------------------------------------------------------------------------------------------------\n";
                    }
                }
            }

            sim.tick();

            uint64_t currentTick = sim.getCurrentTick();
            if (opt.traceEvery > 0 && (currentTick % static_cast<uint64_t>(opt.traceEvery) == 0)) {
                printUnitTrace(sim, ai1, ai2, currentTick);
            }
        }

        // Lấy thông tin kết thúc
        sim::MatchEndInfo endInfo = sim.getMatchEndInfo();

        // Nếu hết tick mà chưa ai thắng thua, kích hoạt timeout check theo tổng quân số
        if (endInfo.result == sim::MatchResult::InProgress && sim.getCurrentTick() >= opt.maxTicks) {
            int pTroops = 0, eTroops = 0;
            for (const auto& u : sim.getUnits()) {
                if (u.isDead()) continue;
                if (u.getFaction() == sim::Faction::Player) pTroops += u.getCurrentUnits();
                else if (u.getFaction() == sim::Faction::Enemy) eTroops += u.getCurrentUnits();
            }
            if (pTroops > eTroops) {
                endInfo.result = sim::MatchResult::Victory;
                endInfo.reason = "Timeout (Max Ticks)! AI1 thang nho quan so (" + std::to_string(pTroops) + " vs " + std::to_string(eTroops) + ")";
            } else if (eTroops > pTroops) {
                endInfo.result = sim::MatchResult::Defeat;
                endInfo.reason = "Timeout (Max Ticks)! AI2 thang nho quan so (" + std::to_string(eTroops) + " vs " + std::to_string(pTroops) + ")";
            } else {
                endInfo.result = sim::MatchResult::Draw;
                endInfo.reason = "Timeout (Max Ticks)! Quan so can bang (" + std::to_string(pTroops) + ")";
            }
        }

        uint64_t finalTick = sim.getCurrentTick();
        totalSimTicks += finalTick;
        auto matchEndTime = std::chrono::steady_clock::now();
        auto matchWallMs = std::chrono::duration_cast<std::chrono::milliseconds>(matchEndTime - matchStartTime).count();

        std::string winnerStr = matchResultToWinnerString(endInfo.result);

        if (endInfo.result == sim::MatchResult::Victory) {
            ai1Wins++;
        } else if (endInfo.result == sim::MatchResult::Defeat) {
            ai2Wins++;
        } else {
            draws++;
        }

        // Phân loại nguyên nhân
        if (endInfo.reason.find("Tu lenh") != std::string::npos ||
            endInfo.reason.find("Commander") != std::string::npos) {
            reasonKillCommander++;
        } else if (endInfo.reason.find("Het thoi gian") != std::string::npos ||
                   endInfo.reason.find("Time") != std::string::npos ||
                   finalTick >= opt.maxTicks) {
            reasonTimeout++;
        } else {
            reasonOutOfTroops++;
        }

        // Thu thập thông số đơn vị cuối trận
        int ai1Alive = 0, ai1Troops = 0, ai1CmdHp = 0;
        int ai2Alive = 0, ai2Troops = 0, ai2CmdHp = 0;
        for (const auto& u : sim.getUnits()) {
            if (u.isDead()) continue;
            if (u.getFaction() == sim::Faction::Player) {
                ai1Alive++;
                ai1Troops += u.getCurrentUnits();
                if (u.getType() == sim::UnitType::Commander) ai1CmdHp = u.getHp();
            } else if (u.getFaction() == sim::Faction::Enemy) {
                ai2Alive++;
                ai2Troops += u.getCurrentUnits();
                if (u.getType() == sim::UnitType::Commander) ai2CmdHp = u.getHp();
            }
        }

        // Ghi Replay nếu có
        if (shouldRecord) {
            recorder.finish(endInfo.result, endInfo.reason, finalTick);
            std::string replayOut = opt.saveReplayFile;
            if (replayOut.empty() && !opt.replayDir.empty()) {
                std::ostringstream ss;
                ss << opt.replayDir << "/match_" << matchIdx << "_seed_" << currentSeed << ".json";
                replayOut = ss.str();
            }
            if (!replayOut.empty()) {
                if (!recorder.saveToFile(replayOut)) {
                    std::cerr << "[Warning] Khong the luu replay ra: " << replayOut << "\n";
                }
            }
        }

        // Ghi CSV log
        if (csvStream.is_open()) {
            csvStream << matchIdx << ","
                     << currentSeed << ","
                     << winnerStr << ","
                     << "\"" << endInfo.reason << "\","
                     << finalTick << ","
                     << (finalTick / 20) << ","
                     << matchWallMs << ","
                     << ai1Alive << "," << ai1Troops << "," << ai1CmdHp << ","
                     << ai2Alive << "," << ai2Troops << "," << ai2CmdHp << ","
                     << ai1.getTotalCommandsIssued() << ","
                     << ai2.getTotalCommandsIssued() << "\n";
            csvStream.flush();
        }

        // In tổng kết ván nếu đang live hoặc quiet
        double matchTps = (matchWallMs > 0) ? (static_cast<double>(finalTick) * 1000.0 / static_cast<double>(matchWallMs)) : 0.0;
        double speedup = matchTps / 20.0;

        if (opt.liveOutput || opt.quietOutput || opt.matches > 1) {
            std::cout << "[Match #" << std::setw(2) << matchIdx << " END] Winner: "
                      << std::left << std::setw(10) << winnerStr
                      << " | Reason: " << endInfo.reason
                      << " | Ticks: " << finalTick << " (" << (finalTick / 20) << "s)"
                      << " | Wall: " << matchWallMs << "ms (" << std::fixed << std::setprecision(0) << matchTps
                      << " tps, " << std::setprecision(1) << speedup << "x realtime)\n";
        }
    }

    auto batchEndTime = std::chrono::steady_clock::now();
    auto totalWallMs = std::chrono::duration_cast<std::chrono::milliseconds>(batchEndTime - batchStartTime).count();
    double totalTps = (totalWallMs > 0) ? (static_cast<double>(totalSimTicks) * 1000.0 / static_cast<double>(totalWallMs)) : 0.0;
    double overallSpeedup = totalTps / 20.0;

    // In bảng tổng kết toàn đợt
    std::cout << "\n========================================================================\n"
              << "                 WAR STRATEGY BATCH RUN SUMMARY                        \n"
              << "========================================================================\n"
              << "Total Matches Completed : " << opt.matches << "\n"
              << "AI 1 (Blue) Victories   : " << ai1Wins << " (" << (opt.matches > 0 ? (ai1Wins * 100 / opt.matches) : 0) << "%)\n"
              << "AI 2 (Red)  Victories   : " << ai2Wins << " (" << (opt.matches > 0 ? (ai2Wins * 100 / opt.matches) : 0) << "%)\n"
              << "Draws                   : " << draws   << " (" << (opt.matches > 0 ? (draws * 100 / opt.matches) : 0) << "%)\n"
              << "------------------------------------------------------------------------\n"
              << "End Reasons Breakdown:\n"
              << "  - Kill Commander      : " << reasonKillCommander << "\n"
              << "  - Out of Troops       : " << reasonOutOfTroops << "\n"
              << "  - Timeout (Max Ticks) : " << reasonTimeout << "\n"
              << "------------------------------------------------------------------------\n"
              << "Simulation Performance:\n"
              << "  - Total Ticks Simulated : " << totalSimTicks << " ticks (" << (totalSimTicks / 20) << "s in-game)\n"
              << "  - Total Wall Time       : " << totalWallMs << " ms (" << std::fixed << std::setprecision(2) << (totalWallMs / 1000.0) << " s)\n"
              << "  - Average Simulation Speed: " << std::fixed << std::setprecision(0) << totalTps << " ticks/sec ("
              << std::setprecision(1) << overallSpeedup << "x REALTIME)\n"
              << "========================================================================\n";

    if (!opt.logCsvFile.empty()) {
        std::cout << "CSV log saved to: " << opt.logCsvFile << "\n";
    }

    return 0;
}
