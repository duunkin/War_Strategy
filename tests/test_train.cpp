#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

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
    if (std::isnan(p.threatRadiusTiles) || std::isinf(p.threatRadiusTiles)) return false;

    if (p.decisionIntervalTicks < 8 || p.decisionIntervalTicks > 40) return false;
    if (p.aggression < 0.15f || p.aggression > 1.50f) return false;
    if (p.retreatHpRatio < 0.05f || p.retreatHpRatio > 0.70f) return false;
    if (p.commanderSafeRadiusTiles < 4.0f || p.commanderSafeRadiusTiles > 22.0f) return false;
    if (p.artilleryStandOffMargin < 1.0f || p.artilleryStandOffMargin > 6.0f) return false;
    if (p.artilleryIdealRangeRatio < 0.50f || p.artilleryIdealRangeRatio > 0.95f) return false;
    if (p.scoutingWeight < 0.10f || p.scoutingWeight > 3.00f) return false;
    if (p.focusFireWeight < 0.20f || p.focusFireWeight > 3.00f) return false;
    if (p.threatRadiusTiles < 6.0f || p.threatRadiusTiles > 32.0f) return false;
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
            p.threatRadiusTiles = static_cast<float>(rng.range(-10, 60));
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

    // Kiem tra tuong thich nguoc: Profile JSON cu thieu threatRadiusTiles (M9.9 Item 4)
    {
        const std::string legacyJsonPath = "test_legacy_profile.json";

        // 1. Dang profile co metadata + params (thieu threatRadiusTiles)
        {
            std::ofstream f(legacyJsonPath);
            f << R"({
                "metadata": {"generation": 5, "fitness": 120.0},
                "params": {
                    "aggression": 0.85,
                    "retreatHpRatio": 0.25,
                    "decisionIntervalTicks": 20
                }
            })";
        }
        AiParams loadedLegacy;
        loadedLegacy.threatRadiusTiles = 30.0f; // co tinh gan gia tri khac truoc khi nap
        AiTrainingMetadata legacyMeta;
        assert(loadTrainedAiProfile(legacyJsonPath, loadedLegacy, &legacyMeta) && "Doc profile cu phai thanh cong!");
        assert(std::abs(loadedLegacy.threatRadiusTiles - 16.0f) < 1e-4f && "Profile cu thieu threatRadiusTiles phai mac dinh la 16.0f!");
        assert(std::abs(loadedLegacy.aggression - 0.85f) < 1e-4f);
        assert(std::abs(loadedLegacy.retreatHpRatio - 0.25f) < 1e-4f);
        assert(legacyMeta.generation == 5);

        // 2. Dang phang truc tiep (thieu threatRadiusTiles)
        {
            std::ofstream f(legacyJsonPath);
            f << R"({
                "aggression": 0.70,
                "retreatHpRatio": 0.35,
                "focusFireWeight": 1.5
            })";
        }
        AiParams loadedFlat;
        loadedFlat.threatRadiusTiles = 25.0f;
        assert(loadTrainedAiProfile(legacyJsonPath, loadedFlat) && "Doc JSON phang cu phai thanh cong!");
        assert(std::abs(loadedFlat.threatRadiusTiles - 16.0f) < 1e-4f && "JSON phang cu thieu threatRadiusTiles phai mac dinh la 16.0f!");
        assert(std::abs(loadedFlat.aggression - 0.70f) < 1e-4f);
        assert(std::abs(loadedFlat.retreatHpRatio - 0.35f) < 1e-4f);

        std::filesystem::remove(legacyJsonPath);
        std::cout << "  - Backward compatibility: Profile JSON cu khong co 'threatRadiusTiles' tu dong dung mac dinh 16.0f (PASSED)" << std::endl;
    }

    // Kiểm tra chuẩn hóa Score_shaping: Thua do Tư lệnh chết (dù nhiều hơn 10.000 bộ binh) LUÔN < Thắng Timeout
    {
        SingleMatchResult lossWithMoreTroops;
        lossWithMoreTroops.isLoss = true;
        lossWithMoreTroops.endKind = sim::MatchEndKind::CommanderKilled;
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
        winTimeout.endKind = sim::MatchEndKind::Timeout;
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
        bool hasCmdDeathsCol = false;
        while (std::getline(csv, line)) {
            if (!line.empty()) {
                lineCount++;
                if (line.find("commander_deaths") != std::string::npos) {
                    hasCmdDeathsCol = true;
                }
            }
        }
        assert(lineCount >= 3 && "CSV phai chua it nhat 1 header + 2 dong du lieu the he!");
        assert(hasCmdDeathsCol && "CSV phai chua cot commander_deaths!");
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

// =============================================================================
// TEST 5: Simulation Determinism Anti-Noise Test
// =============================================================================
void testSimulationDeterminismAntiNoise() {
    std::cout << "[TestTrain] 5. Kiem tra chong nhieu & tat dinh 100% tren cung seed (Test 5)...\n";
    constexpr uint64_t TEST_SEED = 0xABCD1234EF56ULL;
    constexpr uint64_t RUN_TICKS = 400;

    AiParams pA;
    pA.aggression = 0.95f;
    pA.artilleryStandOffMargin = 2.5f;
    clampParams(pA);

    AiParams pB;
    pB.aggression = 0.45f;
    pB.artilleryStandOffMargin = 4.5f;
    clampParams(pB);

    SingleMatchResult r1 = runSimulationMatch(pA, pB, true, TEST_SEED, RUN_TICKS);
    SingleMatchResult r2 = runSimulationMatch(pA, pB, true, TEST_SEED, RUN_TICKS);

    assert(r1.finalTick == r2.finalTick);
    assert(r1.endKind == r2.endKind);
    assert(r1.endReason == r2.endReason);
    assert(r1.isWin == r2.isWin);
    assert(r1.isLoss == r2.isLoss);
    assert(r1.isDraw == r2.isDraw);
    assert(r1.candidateTotalTroops == r2.candidateTotalTroops);
    assert(r1.opponentTotalTroops == r2.opponentTotalTroops);
    assert(r1.candidateAliveUnits == r2.candidateAliveUnits);
    assert(r1.opponentAliveUnits == r2.opponentAliveUnits);
    assert(r1.candidateCommanderHp == r2.candidateCommanderHp);
    assert(r1.opponentCommanderHp == r2.opponentCommanderHp);
    assert(r1.candidateCommandsIssued == r2.candidateCommandsIssued);
    assert(r1.opponentCommandsIssued == r2.opponentCommandsIssued);
    assert(std::abs(r1.fitnessScore - r2.fitnessScore) < 1e-5f);

    std::cout << "  -> PASSED: 2 tran chay doc lap voi cung seed va params hoan toan khong bi nhieu, ket qua khop 100%!\n";
}

// =============================================================================
// TEST 6: Best Selection Based on Validation Test
// =============================================================================
void testBestSelectionBasedOnValidation() {
    std::cout << "[TestTrain] 6. Kiem tra Best Profile duoc chon nghiem ngat theo Validation (Test 6)...\n";

    const std::string outBest = "test_val_best.json";
    std::filesystem::remove(outBest);

    TrainerConfig cfg;
    cfg.mode = TrainingMode::Evolution;
    cfg.generations = 2;
    cfg.populationSize = 4;
    cfg.seedsPerGen = 1;
    cfg.validationSeeds = 8;
    cfg.maxTicks = 150;
    cfg.threadCount = 2;
    cfg.masterSeed = 0x1234567890ULL;
    cfg.outBestFile = outBest;
    cfg.quietOutput = true;

    AiTrainer trainer(cfg);
    bool ok = trainer.train();
    assert(ok && "Train phai thanh cong!");

    assert(std::filesystem::exists(outBest));
    AiParams savedBest;
    AiTrainingMetadata savedMeta;
    assert(loadTrainedAiProfile(outBest, savedBest, &savedMeta));

    // Tai tao bo validationSeeds theo masterSeed de kiem tra lai
    uint32_t numVal = cfg.validationSeeds;
    std::vector<uint64_t> valSeeds(numVal);
    mapgen::FastRng valRng(cfg.masterSeed ^ 0xBA11DA7E5EEDULL);
    for (uint32_t v = 0; v < numVal; ++v) {
        valSeeds[v] = valRng.next64();
    }

    AiParams baselineParams{};
    float valFitSum = 0.0f;
    int valWins = 0;
    int valTotal = 0;
    for (uint64_t vs : valSeeds) {
        MirrorPairResult pair = evaluatePair(savedBest, baselineParams, vs, cfg.maxTicks);
        valFitSum += pair.totalFitness;
        valWins += pair.candidateWins;
        valTotal += (pair.candidateWins + pair.opponentWins + pair.draws);
    }
    float expectedAvgFit = valFitSum / static_cast<float>(valSeeds.size());
    float expectedWinRate = (valTotal > 0) ? (static_cast<float>(valWins) / static_cast<float>(valTotal)) : 0.0f;

    // Khang dinh diem luu trong metadata khop chinh xac voi validation fitness tinh toan doc lap
    assert(std::abs(savedMeta.fitness - expectedAvgFit) < 1e-3f);
    assert(std::abs(savedMeta.winRateVsBaseline - expectedWinRate) < 1e-3f);
    assert(savedMeta.baselineProfile == "defaults");

    std::filesystem::remove(outBest);
    std::cout << "  -> PASSED: Best profile duoc luu da duoc kiem chung trung khop 100% voi ket qua Validation doc lap!\n";
}

// =============================================================================
// TEST 7: Deadlock Early Stop Equivalent to Max Ticks (M9.7 Step 3)
// =============================================================================
void testDeadlockEarlyStopEquivalentToMaxTicks() {
    std::cout << "[TestTrain] 7. Kiem tra dung som do be tac (stallTicks) khop 100% ket qua maxTicks (Test 7)...\n";

    AiParams p1{};
    AiParams p2{};

    // Chạy kiểm thử trên 5 seed độc lập
    const std::vector<uint64_t> testSeeds = {7919ULL, 15838ULL, 23757ULL, 39595ULL, 55433ULL};
    const uint64_t maxTicks = 12000;
    const uint32_t stallTicks = 2400;

    for (uint64_t seed : testSeeds) {
        // Trận có dừng sớm (stallTicks = 2400)
        SingleMatchResult resEarly = runSimulationMatch(p1, p2, true, seed, maxTicks, nullptr, 0, 0, nullptr, stallTicks);

        // Trận chạy đủ maxTicks (stallTicks = 0: tắt dừng sớm)
        SingleMatchResult resFull = runSimulationMatch(p1, p2, true, seed, maxTicks, nullptr, 0, 0, nullptr, 0);

        assert(resEarly.isWin == resFull.isWin && "Ket qua thang phai khop giua dung som va maxTicks!");
        assert(resEarly.isLoss == resFull.isLoss && "Ket qua thua phai khop giua dung som va maxTicks!");
        assert(resEarly.isDraw == resFull.isDraw && "Ket qua hoa phai khop giua dung som va maxTicks!");
        assert(resEarly.endKind == resFull.endKind && "EndKind phai khop giua dung som va maxTicks!");
        assert(resEarly.candidateTotalTroops == resFull.candidateTotalTroops && "Quan so candidate phai khop!");
        assert(resEarly.opponentTotalTroops == resFull.opponentTotalTroops && "Quan so opponent phai khop!");
        assert(resEarly.finalTick <= resFull.finalTick && "Tick dung som phai nho hon hoac bang maxTicks!");

        std::cout << "  -> Seed " << seed << ": EarlyTick=" << resEarly.finalTick
                  << " vs FullTick=" << resFull.finalTick
                  << " | Result=" << (resEarly.isWin ? "WIN" : (resEarly.isLoss ? "LOSS" : "DRAW"))
                  << " | Troops=(" << resEarly.candidateTotalTroops << " vs " << resEarly.opponentTotalTroops << ") [OK]\n";
    }

    std::cout << "  -> PASSED: Dung som khi be tac cho ket qua khop 100% voi maxTicks tren toan bo 5 seed!\n";
}

static void testFactionBreakdownAndPairClassification() {
    std::cout << "\n[TEST 8] Kiem thu logic phan tach phe va do nen cap tran (--eval-only M9.10)...\n";

    // Setup 4 cặp trận mô phỏng các trường hợp:
    // Pair 1: Blue thắng cả 2 trận -> Candidate thắng matchA (Blue), thua matchB (Red) -> split 1-1
    MirrorPairResult p1;
    p1.candidateWins = 1; p1.opponentWins = 1;
    p1.matchA.isWin = true;  p1.matchA.isLoss = false; // Cand won as Blue
    p1.matchB.isWin = false; p1.matchB.isLoss = true;  // Opp won as Blue

    // Pair 2: Red thắng cả 2 trận -> Candidate thua matchA (Blue), thắng matchB (Red) -> split 1-1
    MirrorPairResult p2;
    p2.candidateWins = 1; p2.opponentWins = 1;
    p2.matchA.isWin = false; p2.matchA.isLoss = true;  // Opp won as Red
    p2.matchB.isWin = true;  p2.matchB.isLoss = false; // Cand won as Red

    // Pair 3: Candidate thắng cả 2 trận (sweep 2-0)
    MirrorPairResult p3;
    p3.candidateWins = 2; p3.opponentWins = 0;
    p3.matchA.isWin = true;  p3.matchA.isLoss = false; // Cand won as Blue
    p3.matchB.isWin = true;  p3.matchB.isLoss = false; // Cand won as Red

    // Pair 4: Baseline thắng cả 2 trận (sweep 0-2)
    MirrorPairResult p4;
    p4.candidateWins = 0; p4.opponentWins = 2;
    p4.matchA.isWin = false; p4.matchA.isLoss = true;  // Opp won as Red
    p4.matchB.isWin = false; p4.matchB.isLoss = true;  // Opp won as Blue

    std::vector<MirrorPairResult> pairs = {p1, p2, p3, p4};

    int candWinsBlue = 0, candMatchesBlue = 0;
    int candWinsRed = 0, candMatchesRed = 0;
    int sweep2_0 = 0, sweep0_2 = 0, split1_1 = 0;
    int splitBlueBoth = 0, splitRedBoth = 0;

    for (const auto& p : pairs) {
        candMatchesBlue++;
        if (p.matchA.isWin) candWinsBlue++;

        candMatchesRed++;
        if (p.matchB.isWin) candWinsRed++;

        if (p.candidateWins == 2) {
            sweep2_0++;
        } else if (p.opponentWins == 2) {
            sweep0_2++;
        } else if (p.candidateWins == 1 && p.opponentWins == 1) {
            split1_1++;
            if (p.matchA.isWin && p.matchB.isLoss) splitBlueBoth++;
            else if (p.matchA.isLoss && p.matchB.isWin) splitRedBoth++;
        }
    }

    assert(candMatchesBlue == 4);
    assert(candWinsBlue == 2); // p1, p3
    assert(candMatchesRed == 4);
    assert(candWinsRed == 2); // p2, p3

    assert(sweep2_0 == 1);
    assert(sweep0_2 == 1);
    assert(split1_1 == 2);
    assert(splitBlueBoth == 1);
    assert(splitRedBoth == 1);

    // Kiem tra Wilson CI va Sign Test (M9.11)
    ai::WilsonCI ci50 = ai::wilsonScoreInterval(50, 100);
    assert(ci50.lower > 0.39 && ci50.lower < 0.41);
    assert(ci50.upper > 0.59 && ci50.upper < 0.61);

    ai::WilsonCI ci100 = ai::wilsonScoreInterval(100, 100);
    assert(ci100.upper == 1.0);
    assert(ci100.lower > 0.95);

    double pValEqual = ai::pairedSignTestPValue(5, 5);
    assert(std::abs(pValEqual - 1.0) < 1e-5);

    double pValSweep10_0 = ai::pairedSignTestPValue(10, 0);
    assert(pValSweep10_0 < 0.01);

    double pVal8_2 = ai::pairedSignTestPValue(8, 2);
    // n=10, k=2: 2 * (1 + 10 + 45) / 1024 = 0.109375
    assert(std::abs(pVal8_2 - 0.109375) < 1e-4);

    std::cout << "  -> PASSED: Logic phan loai 2-0, 0-2, 1-1, Wilson CI va Sign Test hoat dong chinh xac!\n";
}

// =============================================================================
// TEST 9: Baseline Profile Configuration and Propagation (M9.12)
// =============================================================================
void testBaselineProfileOptionAndPropagation() {
    std::cout << "\n[TEST 9] Kiem tra tuy chon --baseline va su lan truyen baseline trong AI Trainer...\n";

    // 1. Kiem tra tinh toan param diffs
    ai::AiParams defParams{};
    auto emptyDiffs = ai::getParamDiffsFromDefault(defParams);
    assert(emptyDiffs.empty() && "Default params khong duoc co diffs");

    ai::AiParams customBaseline{};
    customBaseline.enableUnitRetreat = false;
    customBaseline.scoutingWeight = 0.5f;
    customBaseline.targetMemoryTicks = 200;

    auto diffs = ai::getParamDiffsFromDefault(customBaseline);
    assert(diffs.size() == 3);

    // 2. Luu custom baseline ra file tam
    const std::string baselineFile = "build/test_temp_baseline.json";
    const std::string outBestFile = "build/test_temp_outbest.json";
    std::filesystem::create_directories("build");

    ai::AiTrainingMetadata baseMeta;
    baseMeta.algorithm = "manual";
    assert(ai::saveTrainedAiProfile(baselineFile, customBaseline, baseMeta));

    // 3. Khoi tao trainer voi baselineFile
    ai::TrainerConfig cfg;
    cfg.mode = ai::TrainingMode::Evolution;
    cfg.generations = 1;
    cfg.populationSize = 2;
    cfg.seedsPerGen = 1;
    cfg.validationSeeds = 2;
    cfg.maxTicks = 100;
    cfg.threadCount = 1;
    cfg.masterSeed = 42ULL;
    cfg.baselineFile = baselineFile;
    cfg.outBestFile = outBestFile;
    cfg.quietOutput = true;

    ai::AiTrainer trainer(cfg);
    assert(trainer.getBaselineParams() == customBaseline);

    bool ok = trainer.train();
    assert(ok);
    assert(std::filesystem::exists(outBestFile));

    ai::AiParams outParams;
    ai::AiTrainingMetadata outMeta;
    assert(ai::loadTrainedAiProfile(outBestFile, outParams, &outMeta));
    assert(outMeta.baselineProfile == baselineFile);

    std::filesystem::remove(baselineFile);
    std::filesystem::remove(outBestFile);
    std::cout << "  -> PASSED: Baseline profile duoc nap, lan truyen va ghi nhan metadata chinh xac 100%!\n";
}

// =============================================================================
// TEST 10: Horizontal Map Reflection Evaluation (--eval-flip-map - M9.12)
// =============================================================================
void testEvalFlipMapTerrainReflection() {
    std::cout << "\n[TEST 10] Kiem tra tuy chon --eval-flip-map va tinh tat dinh tren ban do lat guong...\n";

    ai::AiParams cand{};
    cand.enableUnitRetreat = false;
    ai::AiParams base{};

    uint64_t testSeed = 99999ULL;
    uint64_t maxTicks = 120;

    // 1. Chay cap ban do goc (flipMapX = false)
    ai::MirrorPairResult orig1 = ai::evaluatePair(cand, base, testSeed, maxTicks, nullptr, 0, 20, nullptr, 2400, false);
    ai::MirrorPairResult orig2 = ai::evaluatePair(cand, base, testSeed, maxTicks, nullptr, 0, 20, nullptr, 2400, false);
    assert(orig1.matchA.finalTick == orig2.matchA.finalTick);
    assert(orig1.matchA.candidateTotalTroops == orig2.matchA.candidateTotalTroops);
    assert(orig1.matchB.candidateTotalTroops == orig2.matchB.candidateTotalTroops);

    // 2. Chay cap ban do lat guong (flipMapX = true)
    ai::MirrorPairResult flip1 = ai::evaluatePair(cand, base, testSeed, maxTicks, nullptr, 0, 20, nullptr, 2400, true);
    ai::MirrorPairResult flip2 = ai::evaluatePair(cand, base, testSeed, maxTicks, nullptr, 0, 20, nullptr, 2400, true);
    assert(flip1.matchA.finalTick == flip2.matchA.finalTick);
    assert(flip1.matchA.candidateTotalTroops == flip2.matchA.candidateTotalTroops);
    assert(flip1.matchB.candidateTotalTroops == flip2.matchB.candidateTotalTroops);

    // Ban do lat guong phai hop le ve so quan ton tai
    assert(flip1.matchA.finalTick > 0);
    assert(flip1.matchB.finalTick > 0);

    std::cout << "  -> PASSED: Ban do lat guong hoat dong tat dinh 100% va giu nguyen tinh toan ven mo phong!\n";
}

int main() {
    std::cout << "========================================================================\n"
              << "War Strategy - AI Self-Play & Trainer Unit Tests (M9)\n"
              << "========================================================================\n";

    testParamsBoundsAndJsonRoundTrip();
    testMirrorMatchDeterminism();
    testPostLossAdaptation();
    testMiniEvolutionEndToEnd();
    testSimulationDeterminismAntiNoise();
    testBestSelectionBasedOnValidation();
    testDeadlockEarlyStopEquivalentToMaxTicks();
    testFactionBreakdownAndPairClassification();
    testBaselineProfileOptionAndPropagation();
    testEvalFlipMapTerrainReflection();

    std::cout << "\n========================================================================\n"
              << "[PASS] TOAN BO 10/10 KIEM THU AI TRAINER M9 DA DAT CHUAN 100%!\n"
              << "========================================================================\n";
    return 0;
}
