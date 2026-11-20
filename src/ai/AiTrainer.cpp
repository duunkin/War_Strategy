#include "AiTrainer.hpp"
#include "TacticalAI.hpp"
#include "../sim/map/WorldMap.hpp"
#include "../mapgen/MapGenerator.hpp"
#include "../mapgen/MapGenConfig.hpp"
#include "../replay/Replay.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include <queue>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace ai {

namespace {

bool isStdoutATty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

std::string getIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

} // namespace

// =============================================================================
// Tính điểm Fitness liên tục có độ phân giải cao
// =============================================================================
float calculateSingleMatchFitness(const SingleMatchResult& res, uint64_t maxTicks) {
    float score = 0.0f;
    float timeRatio = (maxTicks > 0) ? (static_cast<float>(res.finalTick) / static_cast<float>(maxTicks)) : 1.0f;
    timeRatio = std::clamp(timeRatio, 0.0f, 1.0f);
    float speedBonus = 1.0f - timeRatio; // 1.0 nếu thắng ngay lập tức, 0.0 nếu hết giờ

    // 1. Phân loại Thắng / Thua / Hòa
    if (res.isWin) {
        if (res.endReason.find("Tu lenh") != std::string::npos || res.endReason.find("Commander") != std::string::npos) {
            // Thắng bằng giết Tư lệnh địch: +1000 + thưởng kết thúc nhanh
            score += 1000.0f + 500.0f * speedBonus;
        } else if (res.endReason.find("het quan") != std::string::npos || res.endReason.find("wiped") != std::string::npos) {
            // Thắng bằng quét sạch quân: +800 + thưởng kết thúc nhanh
            score += 800.0f + 400.0f * speedBonus;
        } else {
            // Thắng do Timeout (nhiều quân hơn khi hết giờ)
            score += 300.0f;
        }
    } else if (res.isLoss) {
        if (res.endReason.find("Tu lenh") != std::string::npos || res.endReason.find("Commander") != std::string::npos) {
            // Thua do Tư lệnh chết: -1000, phạt nặng hơn nếu chết quá sớm
            score -= (1000.0f + 200.0f * speedBonus);
        } else if (res.endReason.find("het quan") != std::string::npos || res.endReason.find("wiped") != std::string::npos) {
            // Thua do hết quân
            score -= 800.0f;
        } else {
            // Thua do Timeout
            score -= 300.0f;
        }
    } else {
        // Hòa trận
        score += 0.0f;
    }

    // 2. Điểm định hình liên tục (Shaping Score): chuẩn hóa chặt về biên [-200.0, +200.0]
    // Tránh việc chênh lệch 30.000 bộ binh lấn át điểm phân định thắng/thua
    double candTroops = static_cast<double>(res.candidateTotalTroops);
    double oppTroops  = static_cast<double>(res.opponentTotalTroops);
    double candUnits  = static_cast<double>(res.candidateAliveUnits);
    double oppUnits   = static_cast<double>(res.opponentAliveUnits);
    double candCmdHp  = static_cast<double>(res.candidateCommanderHp);
    double oppCmdHp   = static_cast<double>(res.opponentCommanderHp);

    double troopScore = 120.0 * std::clamp((candTroops - oppTroops) / 30000.0, -1.0, 1.0);
    double unitScore  =  40.0 * std::clamp((candUnits  - oppUnits)  / 30.0,    -1.0, 1.0);
    double cmdHpScore =  40.0 * std::clamp((candCmdHp  - oppCmdHp)  / 100.0,   -1.0, 1.0);

    float shaping = static_cast<float>(troopScore + unitScore + cmdHpScore);
    score += shaping;

    return score;
}

// =============================================================================
// Chẩn đoán trận thua (Match Diagnosis)
// =============================================================================
MatchDiagnosis diagnoseMatch(const sim::WorldSim& sim, sim::Faction loserFaction, const std::string& endReason) {
    MatchDiagnosis diag;
    diag.reason = endReason;
    diag.ticksElapsed = sim.getCurrentTick();

    const auto& units = sim.getUnits();
    int friendlyRemainingTroops = 0;
    int enemyRemainingTroops = 0;
    int friendlyCommanderHp = 0;
    bool friendlyCmdAlive = false;

    for (const auto& u : units) {
        if (u.isDead()) continue;
        if (u.getFaction() == loserFaction) {
            friendlyRemainingTroops += u.getCurrentUnits();
            if (u.getType() == sim::UnitType::Commander) {
                friendlyCommanderHp = u.getHp();
                friendlyCmdAlive = true;
            }
        } else {
            enemyRemainingTroops += u.getCurrentUnits();
        }
    }

    diag.finalCommanderHp = friendlyCommanderHp;
    diag.commanderDied = !friendlyCmdAlive;

    // Phân loại nguyên nhân thất bại
    if (!friendlyCmdAlive || endReason.find("Tu lenh") != std::string::npos || endReason.find("Commander") != std::string::npos) {
        diag.category = MatchDiagnosis::LossCategory::CommanderAssassinated;
    } else if (friendlyRemainingTroops < 800 || (enemyRemainingTroops > friendlyRemainingTroops * 2)) {
        diag.category = MatchDiagnosis::LossCategory::TroopAttrition;
    } else {
        diag.category = MatchDiagnosis::LossCategory::OutgunnedOrTimeout;
    }

    return diag;
}

// =============================================================================
// Thích nghi sau trận thua (Post-Loss Adaptation)
// =============================================================================
AiParams adaptParamsAfterLoss(const AiParams& loser, const MatchDiagnosis& diag, mapgen::FastRng& rng) {
    AiParams p = loser;

    switch (diag.category) {
        case MatchDiagnosis::LossCategory::CommanderAssassinated: {
            // Thua do Tư lệnh chết -> Tăng bán kính an toàn, bật rút lui, tăng ưu tiên phòng thủ
            float rDelta = static_cast<float>(rng.range(20, 50)) / 10.0f; // +2.0 .. +5.0 ô
            p.commanderSafeRadiusTiles += rDelta;
            p.enableCommanderRetreat = true;
            p.retreatHpRatio += 0.05f;
            p.focusFireWeight += 0.15f; // Diệt nhanh mối đe dọa gần chỉ huy
            break;
        }

        case MatchDiagnosis::LossCategory::TroopAttrition: {
            // Thua do tổn thất quân quá nhanh -> Tăng ngưỡng rút lui, stand-off pháo binh, giảm hung hăng
            float retDelta = static_cast<float>(rng.range(5, 15)) / 100.0f; // +0.05 .. +0.15
            p.retreatHpRatio += retDelta;
            p.enableUnitRetreat = true;

            float marginDelta = static_cast<float>(rng.range(5, 15)) / 10.0f; // +0.5 .. +1.5 ô
            p.artilleryStandOffMargin += marginDelta;
            p.artilleryIdealRangeRatio += 0.05f;

            if (p.aggression > 0.65f) {
                p.aggression -= 0.15f; // Tránh lao vào tự sát
            }
            break;
        }

        case MatchDiagnosis::LossCategory::OutgunnedOrTimeout:
        default: {
            // Thua do bị áp đảo hỏa lực hoặc hết giờ ít quân hơn -> Tăng hỏa lực, hung hăng, trinh sát
            float focDelta = static_cast<float>(rng.range(15, 35)) / 100.0f; // +0.15 .. +0.35
            p.focusFireWeight += focDelta;

            float aggDelta = static_cast<float>(rng.range(10, 25)) / 100.0f; // +0.10 .. +0.25
            p.aggression += aggDelta;

            p.scoutingWeight += 0.30f;
            p.scoutingGroupCount = std::min(6, p.scoutingGroupCount + 1);

            if (p.decisionIntervalTicks > 12) {
                p.decisionIntervalTicks -= 2; // Quyết định nhanh hơn
            }
            break;
        }
    }

    clampParams(p);
    return p;
}

// =============================================================================
// Chạy 1 trận đấu mô phỏng độc lập (Thread-Safe 100%)
// =============================================================================
SingleMatchResult runSimulationMatch(const AiParams& playerParams,
                                     const AiParams& enemyParams,
                                     bool candidateIsPlayer,
                                     uint64_t seed,
                                     uint64_t maxTicks,
                                     LiveTelemetryWatch* liveWatch,
                                     int matchIndex,
                                     int printEvery,
                                     const std::atomic<bool>* stopRequested) {
    SingleMatchResult res;
    res.candidateIsPlayer = candidateIsPlayer;

    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    if (!sim.loadConfig()) {
        res.endReason = "Config load failed";
        return res;
    }

    mapgen::MapGenConfig cfg;
    mapgen::MapGenerator gen;
    gen.generate(map, seed, cfg);
    sim.spawnDivisions(seed);

    // AI Player (Blue) & AI Enemy (Red) độc lập
    TacticalAI aiPlayer(sim::Faction::Player, seed ^ 0xAAAA5555ULL, playerParams);
    TacticalAI aiEnemy(sim::Faction::Enemy, seed ^ 0x5555AAAAULL, enemyParams);

    for (uint64_t t = 0; t < maxTicks; ++t) {
        if (stopRequested && stopRequested->load()) {
            break;
        }
        if (sim.isGameOver()) {
            break;
        }

        aiPlayer.update(sim);
        aiEnemy.update(sim);
        sim.tick();

        // Gửi telemetry nếu được yêu cầu
        if (liveWatch && printEvery > 0 && ((t % printEvery == 0) || sim.isGameOver())) {
            std::lock_guard<std::mutex> lock(liveWatch->mutex);
            liveWatch->simTick = sim.getCurrentTick();
            liveWatch->matchIndex = matchIndex;
            liveWatch->snapAI1 = aiPlayer.getSnapshot();
            liveWatch->snapAI2 = aiEnemy.getSnapshot();
            liveWatch->hasActiveSnapshot = true;
        }
    }

    res.finalTick = sim.getCurrentTick();
    const auto& endInfo = sim.getMatchEndInfo();
    res.result = endInfo.result;
    res.endReason = endInfo.reason;

    // Thu thập số liệu quân số và máu tư lệnh
    int pUnits = 0, pTroops = 0, pCmdHp = 0;
    int eUnits = 0, eTroops = 0, eCmdHp = 0;

    for (const auto& u : sim.getUnits()) {
        if (u.isDead()) continue;
        if (u.getFaction() == sim::Faction::Player) {
            pUnits++;
            pTroops += u.getCurrentUnits();
            if (u.getType() == sim::UnitType::Commander) {
                pCmdHp = u.getHp();
            }
        } else if (u.getFaction() == sim::Faction::Enemy) {
            eUnits++;
            eTroops += u.getCurrentUnits();
            if (u.getType() == sim::UnitType::Commander) {
                eCmdHp = u.getHp();
            }
        }
    }

    if (candidateIsPlayer) {
        res.candidateAliveUnits   = pUnits;
        res.candidateTotalTroops  = pTroops;
        res.candidateCommanderHp  = pCmdHp;
        res.opponentAliveUnits    = eUnits;
        res.opponentTotalTroops   = eTroops;
        res.opponentCommanderHp   = eCmdHp;
    } else {
        res.candidateAliveUnits   = eUnits;
        res.candidateTotalTroops  = eTroops;
        res.candidateCommanderHp  = eCmdHp;
        res.opponentAliveUnits    = pUnits;
        res.opponentTotalTroops   = pTroops;
        res.opponentCommanderHp   = pCmdHp;
    }

    // Xác định kết quả từ góc nhìn Candidate
    if (res.result == sim::MatchResult::Victory) {
        // Player thắng
        if (candidateIsPlayer) {
            res.isWin = true;
        } else {
            res.isLoss = true;
        }
    } else if (res.result == sim::MatchResult::Defeat) {
        // Player thua -> Enemy thắng
        if (candidateIsPlayer) {
            res.isLoss = true;
        } else {
            res.isWin = true;
        }
    } else if (res.result == sim::MatchResult::Draw) {
        res.isDraw = true;
    } else {
        // Trận đấu kết thúc do chạm trần maxTicks mà sim chưa gọi game over
        if (res.candidateTotalTroops > res.opponentTotalTroops) {
            res.isWin = true;
            res.endReason = "Timeout: Candidate won by troop majority";
        } else if (res.candidateTotalTroops < res.opponentTotalTroops) {
            res.isLoss = true;
            res.endReason = "Timeout: Opponent won by troop majority";
        } else {
            res.isDraw = true;
            res.endReason = "Timeout: Draw with equal troops";
        }
    }

    res.fitnessScore = calculateSingleMatchFitness(res, maxTicks);
    return res;
}

// =============================================================================
// Mirror Match: 2 trận đối xứng trên cùng 1 seed
// =============================================================================
MirrorPairResult evaluatePair(const AiParams& candidate,
                              const AiParams& opponent,
                              uint64_t seed,
                              uint64_t maxTicks,
                              LiveTelemetryWatch* liveWatch,
                              int matchBaseIndex,
                              int printEvery,
                              const std::atomic<bool>* stopRequested) {
    MirrorPairResult pair;
    pair.seed = seed;

    // Trận A: Candidate = Player (Blue), Opponent = Enemy (Red)
    pair.matchA = runSimulationMatch(candidate, opponent, true, seed, maxTicks,
                                     liveWatch, matchBaseIndex, printEvery, stopRequested);

    // Trận B: Opponent = Player (Blue), Candidate = Enemy (Red)
    pair.matchB = runSimulationMatch(opponent, candidate, false, seed, maxTicks,
                                     liveWatch, matchBaseIndex + 1, printEvery, stopRequested);

    pair.totalFitness = pair.matchA.fitnessScore + pair.matchB.fitnessScore;

    if (pair.matchA.isWin) pair.candidateWins++;
    else if (pair.matchA.isLoss) pair.opponentWins++;
    else pair.draws++;

    if (pair.matchB.isWin) pair.candidateWins++;
    else if (pair.matchB.isLoss) pair.opponentWins++;
    else pair.draws++;

    return pair;
}

// =============================================================================
// Class AiTrainer Implementation
// =============================================================================
AiTrainer::AiTrainer(const TrainerConfig& config)
    : m_config(config)
{
    // Giới hạn biên an toàn cho số luồng
    if (m_config.threadCount == 0) {
        m_config.threadCount = std::max(1u, std::thread::hardware_concurrency());
    }

    // Nạp checkpoint từ file resume nếu có
    if (!m_config.resumeFile.empty()) {
        AiParams loadedParams;
        AiTrainingMetadata loadedMeta;
        if (loadTrainedAiProfile(m_config.resumeFile, loadedParams, &loadedMeta)) {
            m_bestParams = loadedParams;
            m_bestMetadata = loadedMeta;
            std::cout << "[AiTrainer] Da nap profile tu " << m_config.resumeFile
                      << " (Gen " << loadedMeta.generation << ", Fit " << loadedMeta.fitness << ")\n";
        } else {
            std::cerr << "[AiTrainer] Khong the nap resume file: " << m_config.resumeFile << ", dung mac dinh M7.\n";
            m_bestParams = AiParams{};
        }
    } else {
        m_bestParams = AiParams{};
    }
    clampParams(m_bestParams);

    m_liveWatch.totalGen = m_config.generations;
    m_liveWatch.bestFitness = -99999.0f;
}

void AiTrainer::requestStop() {
    m_stopRequested.store(true);
}

bool AiTrainer::train() {
    initCsvLog();

    bool ok = false;
    if (m_config.mode == TrainingMode::Evolution) {
        ok = runEvolution();
    } else {
        ok = runAdaptation();
    }

    // Sinh showcase replay nếu được cấu hình
    if (!m_config.showcaseReplayFile.empty()) {
        std::cout << "[AiTrainer] Dang sinh showcase replay tran dau Champion vs Baseline vao "
                  << m_config.showcaseReplayFile << "...\n";
        generateShowcaseReplay(m_bestParams, m_config.masterSeed);
    }

    return ok;
}

void AiTrainer::initCsvLog() {
    if (m_config.logCsvFile.empty()) return;

    try {
        std::filesystem::path p(m_config.logCsvFile);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream f(m_config.logCsvFile);
        if (f.is_open()) {
            f << "gen,best_fitness,avg_fitness,winrate_vs_baseline,matches,wall_ms,"
              << "decisionIntervalTicks,aggression,retreatHpRatio,commanderSafeRadiusTiles,"
              << "artilleryStandOffMargin,artilleryIdealRangeRatio,scoutingWeight,focusFireWeight,"
              << "targetMemoryTicks,scoutingGroupCount\n";
        }
    } catch (...) {}
}

void AiTrainer::logCsvGeneration(uint32_t gen, float bestFit, float avgFit, float winRateVsBase,
                                int matches, double wallMs, const AiParams& bestP) {
    if (m_config.logCsvFile.empty()) return;

    try {
        std::ofstream f(m_config.logCsvFile, std::ios::app);
        if (f.is_open()) {
            f << gen << ","
              << std::fixed << std::setprecision(2) << bestFit << ","
              << avgFit << ","
              << (winRateVsBase * 100.0f) << ","
              << matches << ","
              << std::setprecision(1) << wallMs << ","
              << bestP.decisionIntervalTicks << ","
              << std::setprecision(3) << bestP.aggression << ","
              << bestP.retreatHpRatio << ","
              << bestP.commanderSafeRadiusTiles << ","
              << bestP.artilleryStandOffMargin << ","
              << bestP.artilleryIdealRangeRatio << ","
              << bestP.scoutingWeight << ","
              << bestP.focusFireWeight << ","
              << bestP.targetMemoryTicks << ","
              << bestP.scoutingGroupCount << "\n";
        }
    } catch (...) {}
}

AiParams AiTrainer::crossover(const AiParams& p1, const AiParams& p2, mapgen::FastRng& rng) {
    AiParams child;
    auto pickFloat = [&](float a, float b) {
        float alpha = static_cast<float>(rng.range(0, 1000)) / 1000.0f;
        return a * alpha + b * (1.0f - alpha);
    };

    child.decisionIntervalTicks = (rng.range(0, 1) == 0) ? p1.decisionIntervalTicks : p2.decisionIntervalTicks;
    child.aggression = pickFloat(p1.aggression, p2.aggression);
    child.retreatHpRatio = pickFloat(p1.retreatHpRatio, p2.retreatHpRatio);
    child.commanderSafeRadiusTiles = pickFloat(p1.commanderSafeRadiusTiles, p2.commanderSafeRadiusTiles);
    child.artilleryStandOffMargin = pickFloat(p1.artilleryStandOffMargin, p2.artilleryStandOffMargin);
    child.artilleryIdealRangeRatio = pickFloat(p1.artilleryIdealRangeRatio, p2.artilleryIdealRangeRatio);
    child.scoutingWeight = pickFloat(p1.scoutingWeight, p2.scoutingWeight);
    child.focusFireWeight = pickFloat(p1.focusFireWeight, p2.focusFireWeight);
    child.targetMemoryTicks = (rng.range(0, 1) == 0) ? p1.targetMemoryTicks : p2.targetMemoryTicks;
    child.enableCommanderRetreat = (rng.range(0, 1) == 0) ? p1.enableCommanderRetreat : p2.enableCommanderRetreat;
    child.enableUnitRetreat = (rng.range(0, 1) == 0) ? p1.enableUnitRetreat : p2.enableUnitRetreat;
    child.scoutingGroupCount = (rng.range(0, 1) == 0) ? p1.scoutingGroupCount : p2.scoutingGroupCount;

    clampParams(child);
    return child;
}

AiParams AiTrainer::mutate(const AiParams& p, float mutationRate, mapgen::FastRng& rng) {
    AiParams res = p;
    auto mutateFloat = [&](float& val, float scale) {
        if (static_cast<float>(rng.range(0, 1000)) / 1000.0f < mutationRate) {
            float delta = (static_cast<float>(rng.range(0, 2000)) / 1000.0f - 1.0f) * scale * mutationRate;
            val += delta;
        }
    };

    if (static_cast<float>(rng.range(0, 1000)) / 1000.0f < mutationRate) {
        int d = rng.range(-4, 4);
        int val = static_cast<int>(res.decisionIntervalTicks) + d;
        res.decisionIntervalTicks = static_cast<uint32_t>(std::clamp(val, 8, 40));
    }

    mutateFloat(res.aggression, 0.6f);
    mutateFloat(res.retreatHpRatio, 0.3f);
    mutateFloat(res.commanderSafeRadiusTiles, 8.0f);
    mutateFloat(res.artilleryStandOffMargin, 2.5f);
    mutateFloat(res.artilleryIdealRangeRatio, 0.25f);
    mutateFloat(res.scoutingWeight, 1.2f);
    mutateFloat(res.focusFireWeight, 1.2f);

    if (static_cast<float>(rng.range(0, 1000)) / 1000.0f < mutationRate) {
        int d = rng.range(-40, 40);
        int val = static_cast<int>(res.targetMemoryTicks) + d;
        res.targetMemoryTicks = static_cast<uint32_t>(std::clamp(val, 20, 400));
    }

    if (static_cast<float>(rng.range(0, 1000)) / 1000.0f < mutationRate) {
        res.scoutingGroupCount = std::clamp(res.scoutingGroupCount + rng.range(-1, 1), 1, 6);
    }

    clampParams(res);
    return res;
}

// =============================================================================
// Chế độ 1 — Tiến hóa quần thể (Evolution Mode)
// =============================================================================
bool AiTrainer::runEvolution() {
    mapgen::FastRng rng(m_config.masterSeed);

    // 1. Khởi tạo quần thể
    std::vector<Individual> population(m_config.populationSize);
    population[0].params = m_bestParams; // Cá thể 0 là baseline hoặc từ resume

    for (size_t i = 1; i < population.size(); ++i) {
        population[i].params = mutate(m_bestParams, m_config.mutationRateInitial, rng);
    }

    // Hall of Fame lưu lại các quán quân của các thế hệ trước
    std::vector<AiParams> hallOfFame;
    AiParams baselineParams{};

    float allTimeBestFitness = -999999.0f;
    uint32_t totalMatchesRun = 0;

    auto tStartAll = std::chrono::steady_clock::now();

    for (uint32_t gen = 1; gen <= m_config.generations; ++gen) {
        if (m_stopRequested.load()) {
            std::cout << "\n[TRAIN] Nhan tin hieu dung (SIGINT), dung som tai the he " << gen << "...\n";
            break;
        }

        auto tGenStart = std::chrono::steady_clock::now();

        // Tính tỉ lệ đột biến giảm dần (Annealing)
        float progress = (m_config.generations > 1) ? (static_cast<float>(gen - 1) / static_cast<float>(m_config.generations - 1)) : 1.0f;
        float currentMutationRate = m_config.mutationRateInitial - (m_config.mutationRateInitial - m_config.mutationRateFinal) * progress;

        // Sinh danh sách seed cho thế hệ này (tránh overfitting vào 1 map)
        std::vector<uint64_t> genSeeds(m_config.seedsPerGen);
        for (uint32_t s = 0; s < m_config.seedsPerGen; ++s) {
            genSeeds[s] = rng.next64();
        }

        // Chuẩn bị danh sách đối thủ kiểm tra: Baseline + Hall of Fame (tối đa 3 gần nhất)
        std::vector<AiParams> testOpponents;
        testOpponents.push_back(baselineParams);
        for (size_t h = (hallOfFame.size() > 3 ? hallOfFame.size() - 3 : 0); h < hallOfFame.size(); ++h) {
            testOpponents.push_back(hallOfFame[h]);
        }

        // Tạo danh sách Evaluation Tasks
        struct EvalTask {
            size_t   indIndex;
            AiParams opponentParams;
            bool     isBaseline;
            uint64_t seed;
            int      matchBaseIdx;
        };

        std::vector<EvalTask> tasks;
        int matchCounter = 1;
        for (size_t i = 0; i < population.size(); ++i) {
            for (const auto& opp : testOpponents) {
                bool isBase = (opp == baselineParams);
                for (uint64_t s : genSeeds) {
                    tasks.push_back(EvalTask{i, opp, isBase, s, matchCounter});
                    matchCounter += 2; // Mỗi task là 1 cặp mirror (2 trận)
                }
            }
        }

        uint32_t matchesThisGen = static_cast<uint32_t>(tasks.size() * 2);
        std::atomic<size_t> nextTask{0};
        std::atomic<uint64_t> simTicksTotal{0};
        std::atomic<uint32_t> completedMatches{0};

        // Reset bộ đếm trận của cá thể
        for (auto& ind : population) {
            ind.fitness = 0.0f;
            ind.matchesWon = 0;
            ind.totalMatches = 0;
            ind.winRateVsBaseline = 0.0f;
        }

        std::vector<std::vector<MirrorPairResult>> resultsPerIndividual(population.size());
        std::vector<int> baselineWinsPerInd(population.size(), 0);
        std::vector<int> baselineMatchesPerInd(population.size(), 0);
        std::mutex resultsMutex;

        {
            std::lock_guard<std::mutex> lock(m_liveWatch.mutex);
            m_liveWatch.currentGen = gen;
            m_liveWatch.totalGen = m_config.generations;
            m_liveWatch.currentMatch = 0;
            m_liveWatch.totalMatches = matchesThisGen;
        }

        // Chạy đa luồng đánh giá song song
        auto workerFunc = [&](uint32_t threadId) {
            LiveTelemetryWatch* watch = (threadId == 0) ? &m_liveWatch : nullptr;

            while (!m_stopRequested.load()) {
                size_t taskIdx = nextTask.fetch_add(1);
                if (taskIdx >= tasks.size()) break;

                const auto& task = tasks[taskIdx];
                MirrorPairResult pairRes = evaluatePair(population[task.indIndex].params,
                                                        task.opponentParams,
                                                        task.seed,
                                                        m_config.maxTicks,
                                                        watch,
                                                        task.matchBaseIdx,
                                                        m_config.printEvery,
                                                        &m_stopRequested);

                simTicksTotal.fetch_add(pairRes.matchA.finalTick + pairRes.matchB.finalTick);
                completedMatches.fetch_add(2);

                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    resultsPerIndividual[task.indIndex].push_back(pairRes);
                    if (task.isBaseline) {
                        baselineWinsPerInd[task.indIndex] += pairRes.candidateWins;
                        baselineMatchesPerInd[task.indIndex] += (pairRes.candidateWins + pairRes.opponentWins + pairRes.draws);
                    }
                }

                if (threadId == 0) {
                    std::lock_guard<std::mutex> lock(m_liveWatch.mutex);
                    m_liveWatch.currentMatch = completedMatches.load();
                }
            }
        };

        // Khởi động các worker threads
        std::vector<std::thread> workers;
        uint32_t numThreads = std::max(1u, m_config.threadCount);
        for (uint32_t th = 0; th < numThreads; ++th) {
            workers.emplace_back(workerFunc, th);
        }

        // Luồng chính hiển thị UI / Telemetry trong khi các workers tính toán
        while (completedMatches.load() < matchesThisGen && !m_stopRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            if (m_config.quietOutput) continue;

            auto now = std::chrono::steady_clock::now();
            double elapsedSec = std::chrono::duration<double>(now - tGenStart).count();
            uint64_t currentTicks = simTicksTotal.load();
            double tps = (elapsedSec > 0.05) ? (static_cast<double>(currentTicks) / elapsedSec) : 0.0;
            double realtimeMult = tps / 20.0; // Chuẩn 20 tick/s in-game

            std::lock_guard<std::mutex> lock(m_liveWatch.mutex);
            m_liveWatch.ticksPerSecond = tps;
            m_liveWatch.realtimeMultiplier = realtimeMult;

            // In dòng trạng thái tổng quan
            char statusLine[256];
            std::snprintf(statusLine, sizeof(statusLine),
                "[TRAIN M9] Gen %02u/%02u | Match %2u/%2u | Speed: %5.0f t/s (%3.0fx) | BestFit: %+7.1f | WinVsBase: %5.1f%%",
                gen, m_config.generations,
                m_liveWatch.currentMatch, matchesThisGen,
                tps, realtimeMult,
                m_liveWatch.bestFitness,
                m_liveWatch.winRateVsBaseline * 100.0f
            );

            if (m_config.liveOutput && isStdoutATty() && !m_config.scrollOutput) {
                // Di chuyển con trỏ ghi đè tại chỗ 3 dòng (1 status + 2 telemetry)
                std::cout << "\r" << statusLine << "\n";
                if (m_liveWatch.hasActiveSnapshot) {
                    std::cout << formatTelemetryLine(m_liveWatch.snapAI1, m_liveWatch.matchIndex, m_liveWatch.simTick, 20) << "\n";
                    std::cout << formatTelemetryLine(m_liveWatch.snapAI2, m_liveWatch.matchIndex, m_liveWatch.simTick, 20) << "\n";
                } else {
                    std::cout << "AI1[BLUE] Dang khoi tao mo phong ban do...\n";
                    std::cout << "AI2[RED ] Dang khoi tao mo phong ban do...\n";
                }
                std::cout << "\033[3A\r" << std::flush;
            } else if (m_config.scrollOutput) {
                // Chế độ cuộn
                static auto lastScrollPrint = std::chrono::steady_clock::now();
                if (std::chrono::duration<double>(now - lastScrollPrint).count() > 1.0) {
                    std::cout << "\n--- " << statusLine << " ---\n";
                    if (m_liveWatch.hasActiveSnapshot) {
                        std::cout << formatTelemetryLine(m_liveWatch.snapAI1, m_liveWatch.matchIndex, m_liveWatch.simTick, 20) << "\n";
                        std::cout << formatTelemetryLine(m_liveWatch.snapAI2, m_liveWatch.matchIndex, m_liveWatch.simTick, 20) << "\n";
                    }
                    lastScrollPrint = now;
                }
            }
        }

        // Đợi toàn bộ worker hoàn tất
        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        totalMatchesRun += completedMatches.load();

        // 2. Tổng hợp kết quả từng cá thể trong thế hệ
        float sumFitness = 0.0f;
        for (size_t i = 0; i < population.size(); ++i) {
            const auto& pResults = resultsPerIndividual[i];
            float totalFit = 0.0f;
            int totalWins = 0;
            int totalM = 0;

            for (const auto& pr : pResults) {
                totalFit += pr.totalFitness;
                totalWins += pr.candidateWins;
                totalM += (pr.candidateWins + pr.opponentWins + pr.draws);
            }

            population[i].fitness = (pResults.empty()) ? 0.0f : (totalFit / static_cast<float>(pResults.size()));
            population[i].matchesWon = totalWins;
            population[i].totalMatches = totalM;

            if (baselineMatchesPerInd[i] > 0) {
                population[i].winRateVsBaseline = static_cast<float>(baselineWinsPerInd[i]) / static_cast<float>(baselineMatchesPerInd[i]);
            } else {
                population[i].winRateVsBaseline = 0.0f;
            }

            sumFitness += population[i].fitness;
        }

        // 3. Sắp xếp quần thể giảm dần theo fitness
        std::sort(population.begin(), population.end(), [](const Individual& a, const Individual& b) {
            return a.fitness > b.fitness;
        });

        float genBestFitness = population[0].fitness;
        float genAvgFitness  = sumFitness / static_cast<float>(population.size());
        float genBestWinVsBase = population[0].winRateVsBaseline;

        auto tGenEnd = std::chrono::steady_clock::now();
        double genWallMs = std::chrono::duration<double, std::milli>(tGenEnd - tGenStart).count();

        // Cập nhật liveWatch
        {
            std::lock_guard<std::mutex> lock(m_liveWatch.mutex);
            if (genBestFitness > m_liveWatch.bestFitness) {
                m_liveWatch.bestFitness = genBestFitness;
            }
            m_liveWatch.winRateVsBaseline = genBestWinVsBase;
        }

        // In dòng tổng kết thế hệ rõ ràng ra terminal (xóa sạch 3 dòng live cũ nếu có)
        if (m_config.liveOutput && isStdoutATty() && !m_config.scrollOutput) {
            std::cout << "\n\n\n\r" << std::flush;
        }

        std::cout << ">>> [GEN " << std::setw(2) << gen << "/" << m_config.generations << "] "
                  << "BestFit: " << std::fixed << std::setprecision(1) << genBestFitness << " | "
                  << "AvgFit: " << genAvgFitness << " | "
                  << "WinVsBase: " << std::setprecision(1) << (genBestWinVsBase * 100.0f) << "% | "
                  << "Time: " << std::setprecision(0) << genWallMs << "ms\n";

        // Ghi CSV
        logCsvGeneration(gen, genBestFitness, genAvgFitness, genBestWinVsBase,
                         matchesThisGen, genWallMs, population[0].params);

        // 4. Cập nhật All-Time Best Params và lưu ngay ra đĩa khi phá kỷ lục
        if (genBestFitness > allTimeBestFitness || gen == 1) {
            allTimeBestFitness = genBestFitness;
            m_bestParams = population[0].params;

            m_bestMetadata.generation = gen;
            m_bestMetadata.fitness = genBestFitness;
            m_bestMetadata.winRateVsBaseline = genBestWinVsBase;
            m_bestMetadata.trainedMatches = totalMatchesRun;
            m_bestMetadata.algorithm = "evo";
            m_bestMetadata.timestamp = getIsoTimestamp();

            if (!m_config.outBestFile.empty()) {
                if (saveTrainedAiProfile(m_config.outBestFile, m_bestParams, m_bestMetadata)) {
                    std::cout << "  [*] Pha ky luc fitness! Da cap nhat profile tot nhat vao: "
                              << m_config.outBestFile << "\n";
                }
            }
        }

        // 5. Cập nhật Hall of Fame
        hallOfFame.push_back(population[0].params);

        if (gen == m_config.generations) {
            break;
        }

        // 6. Sinh sản thế hệ mới (Elitism + Crossover + Mutation)
        std::vector<Individual> nextGen(m_config.populationSize);
        uint32_t elite = std::min(m_config.eliteCount, m_config.populationSize);

        // Elitism: giữ nguyên các cá thể đứng đầu
        for (uint32_t e = 0; e < elite; ++e) {
            nextGen[e] = population[e];
        }

        // Sinh các cá thể còn lại
        size_t topPool = std::max<size_t>(2, population.size() / 2);
        for (size_t c = elite; c < nextGen.size(); ++c) {
            size_t p1Idx = rng.range(0, static_cast<int>(topPool - 1));
            size_t p2Idx = rng.range(0, static_cast<int>(topPool - 1));
            if (p1Idx == p2Idx) {
                p2Idx = (p1Idx + 1) % topPool;
            }

            AiParams childParams = crossover(population[p1Idx].params, population[p2Idx].params, rng);
            childParams = mutate(childParams, currentMutationRate, rng);

            nextGen[c].params = childParams;
        }

        population = std::move(nextGen);
    }

    auto tAllEnd = std::chrono::steady_clock::now();
    double totalSeconds = std::chrono::duration<double>(tAllEnd - tStartAll).count();

    std::cout << "\n========================================================================\n"
              << "[TRAIN] HOAN TAT TIEN HOA QUAN THE (EVOLUTION SELF-PLAY)!\n"
              << "  - Tong so tran da dau: " << totalMatchesRun << "\n"
              << "  - Thoi gian thuc hien: " << std::fixed << std::setprecision(1) << totalSeconds << "s\n"
              << "  - Best Fitness dat duoc: " << m_bestMetadata.fitness << "\n"
              << "  - Ty le thang vs Baseline: " << (m_bestMetadata.winRateVsBaseline * 100.0f) << "%\n"
              << "  - Profile xuat sac nhat da luu tai: " << m_config.outBestFile << "\n"
              << "========================================================================\n";

    return true;
}

// =============================================================================
// Chế độ 2 — Tự học thích nghi sau trận thua (Adaptation Mode)
// =============================================================================
bool AiTrainer::runAdaptation() {
    mapgen::FastRng rng(m_config.masterSeed);
    AiParams currentCandidate = m_bestParams;
    AiParams baselineParams{};

    float allTimeBestFitness = -999999.0f;
    uint32_t totalMatchesRun = 0;

    std::cout << "[TRAIN] Bat dau che do Thich nghi sau tran thua (Post-Loss Adaptation)...\n";

    auto tStartAll = std::chrono::steady_clock::now();

    for (uint32_t cycle = 1; cycle <= m_config.generations; ++cycle) {
        if (m_stopRequested.load()) break;

        auto tCycleStart = std::chrono::steady_clock::now();
        uint64_t seed = rng.next64();

        std::cout << "\n[ADAPT] Chu ky " << cycle << "/" << m_config.generations << " | Seed: " << seed << "...\n";

        // Chạy Mirror Match với Baseline
        MirrorPairResult pair = evaluatePair(currentCandidate, baselineParams, seed,
                                             m_config.maxTicks, &m_liveWatch, totalMatchesRun + 1,
                                             m_config.printEvery, &m_stopRequested);
        totalMatchesRun += 2;

        std::cout << "  -> Ket qua Mirror: Thang " << pair.candidateWins
                  << " - Thua " << pair.opponentWins
                  << " - Hoa " << pair.draws
                  << " | Fitness: " << std::fixed << std::setprecision(1) << pair.totalFitness << "\n";

        // Nếu candidate thua ít nhất 1 trận trong cặp hoặc fitness quá thấp
        if (pair.opponentWins > 0 || pair.totalFitness < 200.0f) {
            // Xác định trận thua để chẩn đoán
            const SingleMatchResult& lossMatch = (pair.matchA.isLoss) ? pair.matchA : pair.matchB;

            sim::WorldMap map(128, 96, 32);
            sim::WorldSim sim(map);
            sim.loadConfig();
            mapgen::MapGenConfig cfg;
            mapgen::MapGenerator gen;
            gen.generate(map, seed, cfg);
            sim.spawnDivisions(seed);

            sim::Faction loserFaction = lossMatch.candidateIsPlayer ? sim::Faction::Player : sim::Faction::Enemy;
            MatchDiagnosis diag = diagnoseMatch(sim, loserFaction, lossMatch.endReason);

            const char* diagStr = (diag.category == MatchDiagnosis::LossCategory::CommanderAssassinated) ? "CommanderAssassinated"
                                : (diag.category == MatchDiagnosis::LossCategory::TroopAttrition) ? "TroopAttrition"
                                : "OutgunnedOrTimeout";
            std::cout << "  [*] Chan doan that bai: " << diagStr << " | Reason: " << diag.reason << "\n";

            // Sinh bộ tham số thích nghi có định hướng
            AiParams adapted = adaptParamsAfterLoss(currentCandidate, diag, rng);

            // Chạy trận tái đấu kiểm chứng (Validation Rematch)
            std::cout << "  [*] Chay tai dau kiem chung (Validation Rematch)...\n";
            MirrorPairResult rematch = evaluatePair(adapted, baselineParams, seed,
                                                    m_config.maxTicks, &m_liveWatch, totalMatchesRun + 1,
                                                    m_config.printEvery, &m_stopRequested);
            totalMatchesRun += 2;

            std::cout << "  -> Ket qua Tai dau: Thang " << rematch.candidateWins
                      << " - Thua " << rematch.opponentWins
                      << " | Fitness cu: " << pair.totalFitness
                      << " -> Fitness moi: " << rematch.totalFitness << "\n";

            if (rematch.totalFitness > pair.totalFitness) {
                std::cout << "  [+] Thich nghi thanh cong! Giu lai bo tham so cai tien.\n";
                currentCandidate = adapted;
                pair = rematch;
            } else {
                std::cout << "  [-] Tai dau chua cai thien vuot troi, giu lai bo tham so an toan nhat.\n";
            }
        } else {
            std::cout << "  [+] Candidate xuat sac toan thang ca 2 luot tran, tiep tuc duy tri va toi uu them.\n";
        }

        auto tCycleEnd = std::chrono::steady_clock::now();
        double wallMs = std::chrono::duration<double, std::milli>(tCycleEnd - tCycleStart).count();

        // Ghi log
        float winRate = pair.getWinRate();
        logCsvGeneration(cycle, pair.totalFitness, pair.totalFitness, winRate,
                         2, wallMs, currentCandidate);

        // Cập nhật nếu đạt kỷ lục
        if (pair.totalFitness > allTimeBestFitness || cycle == 1) {
            allTimeBestFitness = pair.totalFitness;
            m_bestParams = currentCandidate;

            m_bestMetadata.generation = cycle;
            m_bestMetadata.fitness = pair.totalFitness;
            m_bestMetadata.winRateVsBaseline = winRate;
            m_bestMetadata.trainedMatches = totalMatchesRun;
            m_bestMetadata.algorithm = "adapt";
            m_bestMetadata.timestamp = getIsoTimestamp();

            if (!m_config.outBestFile.empty()) {
                saveTrainedAiProfile(m_config.outBestFile, m_bestParams, m_bestMetadata);
            }
        }
    }

    auto tAllEnd = std::chrono::steady_clock::now();
    double totalSeconds = std::chrono::duration<double>(tAllEnd - tStartAll).count();

    std::cout << "\n========================================================================\n"
              << "[TRAIN] HOAN TAT THICH NGHI (POST-LOSS ADAPTATION)!\n"
              << "  - Tong so tran da dau: " << totalMatchesRun << "\n"
              << "  - Thoi gian: " << std::fixed << std::setprecision(1) << totalSeconds << "s\n"
              << "  - Best Fitness dat duoc: " << m_bestMetadata.fitness << "\n"
              << "  - Profile da luu tai: " << m_config.outBestFile << "\n"
              << "========================================================================\n";

    return true;
}

// =============================================================================
// Sinh Showcase Replay (Champion AI Blue vs Baseline Red)
// =============================================================================
bool AiTrainer::generateShowcaseReplay(const AiParams& championParams, uint64_t seed) {
    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    if (!sim.loadConfig()) return false;

    mapgen::MapGenConfig cfg;
    mapgen::MapGenerator gen;
    gen.generate(map, seed, cfg);
    sim.spawnDivisions(seed);

    AiParams baselineParams{};
    TacticalAI aiChampion(sim::Faction::Player, seed ^ 0xAAAA5555ULL, championParams);
    TacticalAI aiBaseline(sim::Faction::Enemy, seed ^ 0x5555AAAAULL, baselineParams);

    replay::ReplayRecorder recorder;
    recorder.start(seed, m_config.maxTicks, championParams, baselineParams);

    // Bắt toàn bộ command đưa vào WorldSim
    sim.setCommandHook([&recorder](uint64_t tick, const sim::Command& cmd) {
        recorder.recordCommand(tick, cmd);
    });

    for (uint64_t t = 0; t < m_config.maxTicks; ++t) {
        if (sim.isGameOver()) break;
        aiChampion.update(sim);
        aiBaseline.update(sim);
        sim.tick();
    }

    const auto& endInfo = sim.getMatchEndInfo();
    recorder.finish(endInfo.result, endInfo.reason, sim.getCurrentTick());

    bool ok = recorder.saveToFile(m_config.showcaseReplayFile);
    if (ok) {
        std::cout << "[AiTrainer] Da luu showcase replay thanh cong: " << m_config.showcaseReplayFile
                  << " (" << recorder.getRecordCount() << " commands, "
                  << recorder.getHeader().finalTick << " ticks)\n";
    } else {
        std::cerr << "[AiTrainer] Loi khi luu showcase replay vao: " << m_config.showcaseReplayFile << "\n";
    }
    return ok;
}

} // namespace ai
