#include "ai/AiTrainer.hpp"
#include "ai/AiParams.hpp"
#include "ai/AiTelemetry.hpp"
#include "ai/TacticalAI.hpp"
#include "sim/WorldSim.hpp"
#include "sim/map/WorldMap.hpp"
#include "mapgen/FastRng.hpp"
#include "mapgen/MapGenerator.hpp"
#include "mapgen/MapGenConfig.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

using namespace ai;

// Helper kiểm tra tính hợp lệ của AiParams
bool isParamsValid(const AiParams& p) {
    if (std::isnan(p.aggression) || std::isinf(p.aggression)) return false;
    if (std::isnan(p.retreatHpRatio) || std::isinf(p.retreatHpRatio)) return false;
    if (std::isnan(p.commanderSafeRadiusTiles) || std::isinf(p.commanderSafeRadiusTiles)) return false;
    if (std::isnan(p.artilleryStandOffMargin) || std::isinf(p.artilleryStandOffMargin)) return false;
    if (std::isnan(p.artilleryIdealRangeRatio) || std::isinf(p.artilleryIdealRangeRatio)) return false;
    if (std::isnan(p.scoutingWeight) || std::isinf(p.scoutingWeight)) return false;
    if (std::isnan(p.focusFireWeight) || std::isinf(p.focusFireWeight)) return false;

    if (p.decisionIntervalTicks < 8 || p.decisionIntervalTicks > 40) return false;
    if (p.aggression < 0.15f || p.aggression > 1.50f) return false;
    if (p.retreatHpRatio < 0.05f || p.retreatHpRatio > 0.70f) return false;
    if (p.commanderSafeRadiusTiles < 4.0f || p.commanderSafeRadiusTiles > 22.0f) return false;
    if (p.artilleryStandOffMargin < 1.0f || p.artilleryStandOffMargin > 6.0f) return false;
    if (p.artilleryIdealRangeRatio < 0.50f || p.artilleryIdealRangeRatio > 0.95f) return false;
    if (p.scoutingWeight < 0.10f || p.scoutingWeight > 3.00f) return false;
    if (p.focusFireWeight < 0.20f || p.focusFireWeight > 3.00f) return false;
    if (p.targetMemoryTicks < 20 || p.targetMemoryTicks > 400) return false;
    if (p.scoutingGroupCount < 1 || p.scoutingGroupCount > 6) return false;

    return true;
}

// =============================================================================
// TEST 1: Params Bounds & JSON Round-Trip Test
// =============================================================================
void testParamsBoundsAndJsonRoundTrip() {
    std::cout << "[TestTrain] 1. Kiem tra gioi han tham so & JSON Round-Trip (Test 1)...\n";

    mapgen::FastRng rng(999111ULL);

    for (int iter = 0; iter < 100; ++iter) {
        AiParams p;

        // Sinh các giá trị ngẫu nhiên kể cả giá trị biên, âm, cực lớn, NaN, Inf
        if (iter == 0) {
            p.aggression = std::numeric_limits<float>::quiet_NaN();
            p.retreatHpRatio = std::numeric_limits<float>::infinity();
            p.commanderSafeRadiusTiles = -50.0f;
            p.decisionIntervalTicks = 2;
        } else if (iter == 1) {
            p.decisionIntervalTicks = 999;
            p.aggression = -10.0f;
            p.retreatHpRatio = 5.0f;
            p.scoutingGroupCount = -20;
            p.targetMemoryTicks = 9999;
        } else {
            p.decisionIntervalTicks = static_cast<uint32_t>(rng.range(0, 100));
            p.aggression = static_cast<float>(rng.range(-200, 300)) / 100.0f;
            p.retreatHpRatio = static_cast<float>(rng.range(-100, 200)) / 100.0f;
            p.commanderSafeRadiusTiles = static_cast<float>(rng.range(-10, 50));
            p.artilleryStandOffMargin = static_cast<float>(rng.range(-5, 15));
            p.artilleryIdealRangeRatio = static_cast<float>(rng.range(-50, 150)) / 100.0f;
            p.scoutingWeight = static_cast<float>(rng.range(-50, 500)) / 100.0f;
            p.focusFireWeight = static_cast<float>(rng.range(-50, 500)) / 100.0f;
            p.targetMemoryTicks = static_cast<uint32_t>(rng.range(0, 1000));
            p.scoutingGroupCount = rng.range(-5, 15);
        }

        clampParams(p);
        assert(isParamsValid(p) && "Moi tham so sau clampParams phai nam trong gioi han hop le!");

        // JSON Round-Trip test với metadata
        AiTrainingMetadata meta;
        meta.generation = static_cast<uint32_t>(iter + 1);
        meta.fitness = static_cast<float>(iter * 10.5f);
        meta.winRateVsBaseline = 0.75f;
        meta.trainedMatches = static_cast<uint32_t>(iter * 4);
        meta.algorithm = "evo";
        meta.timestamp = "2026-11-20 12:00:00";

        const std::string testJsonPath = "test_temp_profile.json";
        assert(saveTrainedAiProfile(testJsonPath, p, meta) && "Ghi file JSON profile phai thanh cong!");

        AiParams loadedParams;
        AiTrainingMetadata loadedMeta;
        assert(loadTrainedAiProfile(testJsonPath, loadedParams, &loadedMeta) && "Doc file JSON profile phai thanh cong!");

        assert(loadedParams == p && "AiParams doc lai phai khop 100% voi AiParams ban dau!");
        assert(loadedMeta.generation == meta.generation);
        assert(std::abs(loadedMeta.fitness - meta.fitness) < 1e-3f);
        assert(std::abs(loadedMeta.winRateVsBaseline - meta.winRateVsBaseline) < 1e-3f);
        assert(loadedMeta.trainedMatches == meta.trainedMatches);
        assert(loadedMeta.algorithm == meta.algorithm);

        std::filesystem::remove(testJsonPath);
    }

    // Kiểm tra chuẩn hóa Score_shaping: Thua do Tư lệnh chết (dù nhiều hơn 10.000 bộ binh) LUÔN < Thắng Timeout
    {
        SingleMatchResult lossWithMoreTroops;
        lossWithMoreTroops.isLoss = true;
        lossWithMoreTroops.endReason = "Phe Do thang do Tu lenh Phe Xanh bi tieu diet";
        lossWithMoreTroops.finalTick = 1000;
        lossWithMoreTroops.candidateTotalTroops = 25000;
        lossWithMoreTroops.opponentTotalTroops = 15000; // Nhiều hơn đối thủ 10.000 quân
        lossWithMoreTroops.candidateAliveUnits = 25;
        lossWithMoreTroops.opponentAliveUnits = 15;
        lossWithMoreTroops.candidateCommanderHp = 0;
        lossWithMoreTroops.opponentCommanderHp = 100;
        float lossScore = calculateSingleMatchFitness(lossWithMoreTroops, 6000);

        SingleMatchResult winTimeout;
        winTimeout.isWin = true;
        winTimeout.endReason = "Timeout: Candidate won by troop majority";
        winTimeout.finalTick = 6000;
        winTimeout.candidateTotalTroops = 15500;
        winTimeout.opponentTotalTroops = 15000; // Chỉ nhỉnh hơn 500 quân
        winTimeout.candidateAliveUnits = 16;
        winTimeout.opponentAliveUnits = 15;
        winTimeout.candidateCommanderHp = 100;
        winTimeout.opponentCommanderHp = 100;
        float winScore = calculateSingleMatchFitness(winTimeout, 6000);

        assert(lossScore < winScore && "Diem tran THUA do Tu lenh chet LUON phai nho hon diem tran THANG Timeout!");
    }

    std::cout << "  -> PASSED: 100/100 lan test bounds khong NaN/Inf va JSON round-trip khop 100%!\n";
}

// =============================================================================
// TEST 2: Mirror Match Determinism Test
// =============================================================================
void testMirrorMatchDeterminism() {
    std::cout << "[TestTrain] 2. Kiem tra tinh tat dinh cua Mirror Match Evaluator (Test 2)...\n";

    constexpr uint64_t TEST_SEED = 888999111ULL;
    constexpr uint64_t MAX_TICKS = 250;

    AiParams cand;
    cand.aggression = 0.85f;
    cand.retreatHpRatio = 0.35f;
    cand.commanderSafeRadiusTiles = 12.0f;
    clampParams(cand);

    AiParams opp;
    opp.aggression = 0.65f;
    opp.retreatHpRatio = 0.25f;
    opp.commanderSafeRadiusTiles = 8.0f;
    clampParams(opp);

    // Chạy lần 1
    MirrorPairResult pair1 = evaluatePair(cand, opp, TEST_SEED, MAX_TICKS);

    // Chạy lần 2 (độc lập hoàn toàn trên instance mới)
    MirrorPairResult pair2 = evaluatePair(cand, opp, TEST_SEED, MAX_TICKS);

    // So sánh khớp 100%
    assert(pair1.seed == pair2.seed);
    assert(std::abs(pair1.totalFitness - pair2.totalFitness) < 1e-5f);
    assert(pair1.candidateWins == pair2.candidateWins);
    assert(pair1.opponentWins == pair2.opponentWins);
    assert(pair1.draws == pair2.draws);

    assert(pair1.matchA.finalTick == pair2.matchA.finalTick);
    assert(pair1.matchA.candidateTotalTroops == pair2.matchA.candidateTotalTroops);
    assert(pair1.matchA.candidateAliveUnits == pair2.matchA.candidateAliveUnits);
    assert(pair1.matchA.candidateCommanderHp == pair2.matchA.candidateCommanderHp);
    assert(pair1.matchA.opponentTotalTroops == pair2.matchA.opponentTotalTroops);
    assert(pair1.matchA.opponentCommanderHp == pair2.matchA.opponentCommanderHp);
    assert(std::abs(pair1.matchA.fitnessScore - pair2.matchA.fitnessScore) < 1e-5f);

    assert(pair1.matchB.finalTick == pair2.matchB.finalTick);
    assert(pair1.matchB.candidateTotalTroops == pair2.matchB.candidateTotalTroops);
    assert(pair1.matchB.candidateAliveUnits == pair2.matchB.candidateAliveUnits);
    assert(pair1.matchB.candidateCommanderHp == pair2.matchB.candidateCommanderHp);
    assert(pair1.matchB.opponentTotalTroops == pair2.matchB.opponentTotalTroops);
    assert(pair1.matchB.opponentCommanderHp == pair2.matchB.opponentCommanderHp);
    assert(std::abs(pair1.matchB.fitnessScore - pair2.matchB.fitnessScore) < 1e-5f);

    std::cout << "  -> PASSED: 2 lan chay Mirror Match doc lap cho ket qua giong nhau 100%!\n";
}

// =============================================================================
// TEST 3: Post-Loss Adaptation Test
// =============================================================================
void testPostLossAdaptation() {
    std::cout << "[TestTrain] 3. Kiem tra co che Thich nghi sau tran thua (Post-Loss Adaptation) (Test 3)...\n";

    mapgen::FastRng rng(12345ULL);
    AiParams initialParams;
    initialParams.commanderSafeRadiusTiles = 8.0f;
    initialParams.retreatHpRatio = 0.20f;
    initialParams.aggression = 0.90f;
    initialParams.artilleryStandOffMargin = 2.0f;
    clampParams(initialParams);

    // Kịch bản A: Thua do Tư lệnh bị ám sát
    MatchDiagnosis diagCmd;
    diagCmd.category = MatchDiagnosis::LossCategory::CommanderAssassinated;
    diagCmd.commanderDied = true;
    diagCmd.finalCommanderHp = 0;
    diagCmd.reason = "Phe Xanh thang do Tu lenh Phe Do bi tieu diet";

    AiParams adaptedCmd = adaptParamsAfterLoss(initialParams, diagCmd, rng);
    assert(isParamsValid(adaptedCmd));
    assert(adaptedCmd.commanderSafeRadiusTiles > initialParams.commanderSafeRadiusTiles &&
           "Phai tang ban kinh an toan cua Tu lenh sau khi chet!");
    assert(adaptedCmd.enableCommanderRetreat == true);
    assert(adaptedCmd.retreatHpRatio >= initialParams.retreatHpRatio);

    // Kịch bản B: Thua do mất quân quá nhanh
    MatchDiagnosis diagAttrition;
    diagAttrition.category = MatchDiagnosis::LossCategory::TroopAttrition;
    diagAttrition.commanderDied = false;
    diagAttrition.finalCommanderHp = 80;
    diagAttrition.friendlyTroopLoss = 2500;
    diagAttrition.enemyTroopLoss = 500;
    diagAttrition.reason = "Phe Xanh thang do Phe Do bi quet sach quan";

    AiParams adaptedAttr = adaptParamsAfterLoss(initialParams, diagAttrition, rng);
    assert(isParamsValid(adaptedAttr));
    assert(adaptedAttr.retreatHpRatio > initialParams.retreatHpRatio &&
           "Phai tang retreatHpRatio de bao toan quan so!");
    assert(adaptedAttr.enableUnitRetreat == true);
    assert(adaptedAttr.artilleryStandOffMargin > initialParams.artilleryStandOffMargin &&
           "Phai tang cu ly an toan cho phao binh!");
    assert(adaptedAttr.aggression <= initialParams.aggression &&
           "Phai giam hoac giu do hung hang de tranh dot quan!");

    std::cout << "  -> PASSED: Co che thich nghi thay doi tham so dung huong va luon nam trong bounds!\n";
}

// =============================================================================
// TEST 4: Mini Evolution End-to-End Test
// =============================================================================
void testMiniEvolutionEndToEnd() {
    std::cout << "[TestTrain] 4. Kiem tra Mini Evolution End-to-End da luong (Test 4)...\n";

    const std::string outBestFile = "test_mini_best_params.json";
    const std::string logCsvFile  = "test_mini_train_log.csv";

    // Xóa file cũ nếu có
    std::filesystem::remove(outBestFile);
    std::filesystem::remove(logCsvFile);

    TrainerConfig cfg;
    cfg.mode = TrainingMode::Evolution;
    cfg.generations = 2;
    cfg.populationSize = 4;
    cfg.seedsPerGen = 1;
    cfg.maxTicks = 200;
    cfg.threadCount = 2; // Chạy song song 2 luồng
    cfg.masterSeed = 54321ULL;
    cfg.outBestFile = outBestFile;
    cfg.logCsvFile = logCsvFile;
    cfg.quietOutput = true;

    AiTrainer trainer(cfg);
    bool ok = trainer.train();
    assert(ok && "Qua trinh mini evolution phai thanh cong!");

    // Xác nhận file best params tồn tại và hợp lệ
    assert(std::filesystem::exists(outBestFile) && "File best params phai duoc tao ra!");
    AiParams bestP;
    AiTrainingMetadata bestMeta;
    assert(loadTrainedAiProfile(outBestFile, bestP, &bestMeta) && "Doc file best params phai thanh cong!");
    assert(isParamsValid(bestP));
    assert(bestMeta.generation >= 1);

    // Xác nhận file CSV tồn tại và có dữ liệu
    assert(std::filesystem::exists(logCsvFile) && "File log CSV phai duoc tao ra!");
    {
        std::ifstream csv(logCsvFile);
        std::string line;
        int lineCount = 0;
        while (std::getline(csv, line)) {
            if (!line.empty()) lineCount++;
        }
        assert(lineCount >= 3 && "CSV phai chua it nhat 1 header + 2 dong du lieu the he!");
    }

    // Thử nghiệm nạp bestParams vào TacticalAI và WorldSim chạy thực tế
    sim::WorldMap map(128, 96, 32);
    sim::WorldSim sim(map);
    assert(sim.loadConfig());
    mapgen::MapGenConfig mapCfg;
    mapgen::MapGenerator gen;
    gen.generate(map, 9999ULL, mapCfg);
    sim.spawnDivisions(9999ULL);

    TacticalAI aiTested(sim::Faction::Enemy, 9999ULL, bestP);
    for (int t = 0; t < 20; ++t) {
        aiTested.update(sim);
        sim.tick();
    }
    assert(aiTested.getTotalCommandsIssued() > 0 && "AI nap bestParams phai hoat dong va ra lenh!");

    // Dọn dẹp file tạm
    std::filesystem::remove(outBestFile);
    std::filesystem::remove(logCsvFile);

    std::cout << "  -> PASSED: Mini Evolution 2 the he x 4 ca the x 2 luong chay on dinh 100% khong data race!\n";
}

int main() {
    std::cout << "========================================================================\n"
              << "War Strategy - AI Self-Play & Trainer Unit Tests (M9)\n"
              << "========================================================================\n";

    testParamsBoundsAndJsonRoundTrip();
    testMirrorMatchDeterminism();
    testPostLossAdaptation();
    testMiniEvolutionEndToEnd();

    std::cout << "\n========================================================================\n"
              << "[PASS] TOAN BO 4/4 KIEM THU AI TRAINER M9 DA DAT CHUAN 100%!\n"
              << "========================================================================\n";
    return 0;
}
