#include "../ai/AiTrainer.hpp"
#include "../ai/AiTelemetry.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <csignal>
#include <thread>
#include <filesystem>
#include <atomic>
#include <cmath>
#include <algorithm>

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

ai::AiTrainer* g_activeTrainer = nullptr;
std::atomic<bool>* g_evalStopRequested = nullptr;

void sigintHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\n[Ctrl+C] Nhan tin hieu ngat (SIGINT)! Dang dung muot va luu ket qua...\n" << std::flush;
        if (g_activeTrainer) {
            g_activeTrainer->requestStop();
        }
        if (g_evalStopRequested) {
            g_evalStopRequested->store(true);
        }
    }
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

void printHelp(const char* progName) {
    std::cout << "========================================================================\n"
              << "War Strategy - AI Self-Play & Evolutionary Trainer (Milestone 9)\n"
              << "========================================================================\n"
              << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Options Huan Luyen:\n"
              << "  --mode <evo|adapt>           Thuat toan huan luyen: evo (tien hoa quan the) hoac adapt (thich nghi sau tran thua)\n"
              << "                               (Mac dinh: evo)\n"
              << "  --generations <N>            So the he tien hoa hoac chu ky thich nghi (Mac dinh: 10)\n"
              << "  --population <N>             Kich thuoc quan the moi the he (Mac dinh: 8)\n"
              << "  --seeds-per-gen <N>          So seed ban do moi the he (Mac dinh: 6)\n"
              << "  --validation-seeds <N>       So seed validation co dinh danh gia top-3 vs baseline (Mac dinh: 8)\n"
              << "  --max-ticks <N>              Gioi han tick toi da moi tran (Mac dinh: 24000 = 20 phut in-game)\n"
              << "  --threads <N>                So luong CPU chay song song (Mac dinh: hardware_concurrency / procs)\n"
              << "  --procs, --expected-procs <N> So tien trinh train du kien chay song song de tinh threads (Mac dinh: 1)\n"
              << "  --seed <uint64>              Master seed tat dinh cho toan bo qua trinh train (Mac dinh: 12345)\n"
              << "  --resume <file.json>         Tiep tuc huan luyen tu mot file profile AiParams da luu\n"
              << "  --baseline <file.json>       Nap profile lam doi thu chuan/baseline (Mac dinh: AiParams{} mac dinh)\n"
              << "  --out-best <file.json>       Duong dan luu profile bo tham so manh nhat (Mac dinh: data/ai_best_params.json)\n"
              << "                               (Luu y: Bat buoc khac nhau khi chay nhieu tien trinh song song)\n"
              << "  --log <file.csv>             Ghi log CSV lich su chi tiet tung the he / chu ky (ghi noi neu file da ton tai)\n"
              << "  --save-showcase-replay <f>   Sau khi train xong, chay 1 tran Champion AI vs Baseline va luu replay JSON\n"
              << "  --live                       Che do cap nhat truc quan tai cho tren CMD qua ANSI (Mac dinh neu la TTY)\n"
              << "  --scroll                     Che do in cuon tung cap dong kem trang thai\n"
              << "  --quiet                      Tat hien thi telemetry truc quan, chi in tong ket\n"
              << "  --print-every <ticks>        Tan suat lay mau suy nghi cua AI tu Worker 0 (Mac dinh: 20 ticks)\n\n"
              << "Options Danh Gia Doc Lap (--eval-only):\n"
              << "  --eval-only <file.json>      Chi danh gia 1 profile AI tren tap seed moi (khong train)\n"
              << "  --vs-baseline                Danh gia profile doi dau voi Baseline AI (dung kem --eval-only)\n"
              << "  --seeds <N>                  So seed mirror danh gia trong --eval-only (Mac dinh: 8)\n"
              << "  --seed-base <N>              Gia tri goc de tinh dai seed danh gia (Mac dinh: 0)\n"
              << "  --eval-flip-map              Chay them ban do lat guong truc X (x -> W-1-x) va bao rieng ket qua theo ban do (dung kem --eval-only)\n\n"
              << "Tro Giup:\n"
              << "  --help, -h                   Hien thi huong dan su dung nay\n"
              << "========================================================================\n";
}

void printBaselineInfo(const std::string& baselinePath, const ai::AiParams& baselineParams) {
    auto diffs = ai::getParamDiffsFromDefault(baselineParams);
    std::cout << "  - Doi thu chuan (Baseline):   "
              << (baselinePath.empty() ? "defaults (AiParams{})" : baselinePath) << "\n";
    if (diffs.empty()) {
        std::cout << "    * Tham so khac mac dinh:    (Khong co - 100% giong defaults)\n";
    } else {
        std::cout << "    * Tham so khac mac dinh (" << diffs.size() << " tham so):\n";
        for (const auto& df : diffs) {
            std::cout << "      - " << df.name << ": default = " << df.defaultValue
                      << ", baseline = " << df.currentValue << "\n";
        }
    }
}

struct EvalMatchStats {
    int totalMatches = 0;
    int candidateWins = 0;
    int opponentWins = 0;
    int draws = 0;
    int candidateCmdDeaths = 0;
    int opponentCmdDeaths = 0;
    int timeoutMatches = 0;
    float totalFitnessSum = 0.0f;

    int candMatchesBlue = 0;
    int candWinsBlue = 0;
    int candMatchesRed = 0;
    int candWinsRed = 0;

    int pairsSweep2_0 = 0;
    int pairsSweep0_2 = 0;
    int pairsSplit1_1 = 0;
    int pairsSplit1_1_BlueBoth = 0;
    int pairsSplit1_1_RedBoth  = 0;
    int pairsOther = 0;

    void addPair(const ai::MirrorPairResult& pair) {
        if (pair.seed == 0 && pair.matchA.finalTick == 0) return;
        totalMatches += 2;
        candidateWins += pair.candidateWins;
        opponentWins += pair.opponentWins;
        draws += pair.draws;
        totalFitnessSum += pair.totalFitness;

        candMatchesBlue++;
        if (pair.matchA.isWin) candWinsBlue++;

        candMatchesRed++;
        if (pair.matchB.isWin) candWinsRed++;

        if (pair.candidateWins == 2) {
            pairsSweep2_0++;
        } else if (pair.opponentWins == 2) {
            pairsSweep0_2++;
        } else if (pair.candidateWins == 1 && pair.opponentWins == 1) {
            pairsSplit1_1++;
            if (pair.matchA.isWin && pair.matchB.isLoss) {
                pairsSplit1_1_BlueBoth++;
            } else if (pair.matchA.isLoss && pair.matchB.isWin) {
                pairsSplit1_1_RedBoth++;
            }
        } else {
            pairsOther++;
        }

        auto checkMatch = [&](const ai::SingleMatchResult& m) {
            if (m.didCandidateCommanderDie()) candidateCmdDeaths++;
            if (m.opponentCommanderHp <= 0 || (m.isWin && m.endKind == sim::MatchEndKind::CommanderKilled)) {
                opponentCmdDeaths++;
            }
            if (m.endKind == sim::MatchEndKind::Timeout) {
                timeoutMatches++;
            }
        };
        checkMatch(pair.matchA);
        checkMatch(pair.matchB);
    }

    void print(const std::string& sectionTitle, bool allowPooledConclusion = true) const {
        float winrate = (totalMatches > 0) ? (static_cast<float>(candidateWins) / static_cast<float>(totalMatches)) : 0.0f;
        float winrateBlue = (candMatchesBlue > 0) ? (static_cast<float>(candWinsBlue) / static_cast<float>(candMatchesBlue)) : 0.0f;
        float winrateRed = (candMatchesRed > 0) ? (static_cast<float>(candWinsRed) / static_cast<float>(candMatchesRed)) : 0.0f;
        float avgFit = (totalMatches > 0) ? (totalFitnessSum / static_cast<float>(totalMatches / 2)) : 0.0f;
        int totalPairs = totalMatches / 2;

        std::cout << "\n========================================================================\n"
                  << sectionTitle << "\n"
                  << "========================================================================\n"
                  << "  - Tong so tran:               " << totalMatches << " (" << totalPairs << " cap seed)\n"
                  << "  - Ket qua (Thang - Thua - Hoa): " << candidateWins << " - " << opponentWins << " - " << draws << "\n"
                  << "  - Ty le thang tong:           " << std::fixed << std::setprecision(1) << (winrate * 100.0f) << "%\n"
                  << "------------------------------------------------------------------------\n"
                  << "  - Winrate khi Candidate la Blue: " << std::setprecision(1) << (winrateBlue * 100.0f) << "% (" << candWinsBlue << "/" << candMatchesBlue << ")\n"
                  << "  - Winrate khi Candidate la Red : " << std::setprecision(1) << (winrateRed * 100.0f) << "% (" << candWinsRed << "/" << candMatchesRed << ")\n"
                  << "------------------------------------------------------------------------\n"
                  << "  - Do nen phep do (Phan bo ket qua theo cap Seed):\n"
                  << "    * Cap 2-0 (Candidate sweep thang ca 2 tran): " << pairsSweep2_0 << " (" << (totalPairs > 0 ? (pairsSweep2_0 * 100.0f / totalPairs) : 0.0f) << "%)\n"
                  << "    * Cap 0-2 (Baseline sweep thang ca 2 tran):  " << pairsSweep0_2 << " (" << (totalPairs > 0 ? (pairsSweep0_2 * 100.0f / totalPairs) : 0.0f) << "%)\n"
                  << "    * Cap 1-1 (Hai tran cung thuoc ve 1 phe):   " << pairsSplit1_1 << " (" << (totalPairs > 0 ? (pairsSplit1_1 * 100.0f / totalPairs) : 0.0f) << "%)\n"
                  << "      + Trong do Blue thang ca 2 tran:          " << pairsSplit1_1_BlueBoth << "\n"
                  << "      + Trong do Red thang ca 2 tran:           " << pairsSplit1_1_RedBoth << "\n";
        if (pairsOther > 0) {
            std::cout << "    * Cap co ket qua hoa:                       " << pairsOther << " (" << (pairsOther * 100.0f / totalPairs) << "%)\n";
        }
        std::cout << "------------------------------------------------------------------------\n"
                  << "  - Tu lenh Candidate chet:     " << candidateCmdDeaths << " / " << totalMatches << " tran\n"
                  << "  - Tu lenh Baseline chet:      " << opponentCmdDeaths << " / " << totalMatches << " tran\n"
                  << "  - Tran ket thuc do het gio:   " << timeoutMatches << " / " << totalMatches << " tran\n"
                  << "  - Fitness trung binh moi seed: " << std::setprecision(1) << avgFit << "\n"
                  << "------------------------------------------------------------------------\n";

        ai::WilsonCI ci = ai::wilsonScoreInterval(candidateWins, totalMatches);
        double pValue = ai::pairedSignTestPValue(pairsSweep2_0, pairsSweep0_2);

        std::cout << "  THONG KE (M9.11):\n"
                  << "  - Winrate:        " << std::setprecision(1) << (winrate * 100.0f) << "% ("
                  << candidateWins << "/" << totalMatches << " tran)\n"
                  << "  - Wilson CI 95%:  [" << std::setprecision(1) << (ci.lower * 100.0) << "%, "
                  << std::setprecision(1) << (ci.upper * 100.0) << "%]\n";

        if (allowPooledConclusion) {
            std::cout << "  - Sign Test (cap 2-0 vs 0-2): "
                      << pairsSweep2_0 << " vs " << pairsSweep0_2
                      << ", p-value = " << std::setprecision(4) << pValue << "\n";

            if (pValue < 0.05 && ci.lower > 0.50) {
                std::cout << "  >>> KET LUAN: TOT HON Baseline (p<0.05, CI lower > 50%)\n";
            } else if (std::abs(winrate - 0.5f) < 0.001f && pairsSweep2_0 == pairsSweep0_2) {
                std::cout << "  >>> KET LUAN: CAN BANG voi Baseline\n";
            } else {
                std::cout << "  >>> KET LUAN: CHUA KET LUAN (can them du lieu hoac p >= 0.05)\n";
            }
        } else {
            std::cout << "  - Sign Test gop cap: [Bo qua ket luan do vi pham gia dinh doc lap giua ban do goc va lat]\n"
                      << "  - Chu thich: Ban do goc va ban do lat dung chung seed nen khong doc lap; xem ket luan Sign Test theo SEED ben duoi.\n";
        }
        std::cout << "========================================================================\n";
    }
};

int runEvaluationOnly(const std::string& evalFile, bool /*vsBaseline*/, uint32_t numSeeds,
                      const ai::TrainerConfig& config, uint64_t seedBase) {
    if (!std::filesystem::exists(evalFile)) {
        std::cerr << "[ERROR] File profile khong ton tai: " << evalFile << "\n";
        return 1;
    }

    ai::AiParams evalParams;
    ai::AiTrainingMetadata evalMeta;
    if (!ai::loadTrainedAiProfile(evalFile, evalParams, &evalMeta)) {
        std::cerr << "[ERROR] Khong the nap du lieu profile JSON: " << evalFile << "\n";
        return 1;
    }

    ai::AiParams opponentParams{}; // Baseline AI
    if (!config.baselineFile.empty()) {
        if (!ai::loadTrainedAiProfile(config.baselineFile, opponentParams)) {
            std::cerr << "[ERROR] Khong the nap baseline profile JSON: " << config.baselineFile << "\n";
            return 1;
        }
    }
    const std::string opponentName = config.baselineFile.empty() ? "Baseline AI (defaults)" : ("Baseline AI (" + config.baselineFile + ")");

    std::cout << "========================================================================\n"
              << "War Strategy - Danh Gia Profile Doc Lap (--eval-only)\n"
              << "========================================================================\n"
              << "  - Duong dan profile:          " << evalFile << "\n"
              << "  - Duong dan baseline:         " << (config.baselineFile.empty() ? "defaults" : config.baselineFile) << "\n";
    printBaselineInfo(config.baselineFile, opponentParams);
    std::cout << "  - Seed-base:                  " << seedBase << "\n"
              << "  - So seed:                    " << numSeeds << " ("
              << (config.evalFlipMap ? (numSeeds * 4) : (numSeeds * 2)) << " tran "
              << (config.evalFlipMap ? "[Original + Flipped X]" : "[Mirror]") << ")\n"
              << "  - Co flip-map:                " << (config.evalFlipMap ? "BAT (x -> W-1-x)" : "TAT") << "\n"
              << "  - Max Ticks moi tran:         " << config.maxTicks << " (~" << (config.maxTicks / 20) << "s in-game)\n"
              << "  - So luong CPU:               " << config.threadCount << "\n"
              << "  - Master Seed danh gia:       " << config.masterSeed << "\n"
              << "========================================================================\n"
              << "Dang tien hanh danh gia cac cap tran song song...\n" << std::flush;

    // Metadata profile
    std::cout << "  [Profile Metadata] generation=" << evalMeta.generation
              << ", fitness=" << std::setprecision(1) << evalMeta.fitness
              << ", winRateVsBaseline=" << evalMeta.winRateVsBaseline
              << ", algorithm=" << evalMeta.algorithm
              << ", timestamp=" << evalMeta.timestamp << "\n" << std::flush;

    mapgen::FastRng rng((config.masterSeed ^ 0xBA5E114E5EEDULL) + seedBase);
    std::vector<uint64_t> evalSeeds(numSeeds);
    for (uint32_t s = 0; s < numSeeds; ++s) {
        evalSeeds[s] = rng.next64();
    }

    struct EvalPairTask {
        uint64_t seed{0};
        size_t resultIdx{0};
        bool isFlipped{false};
    };

    std::vector<EvalPairTask> tasks;
    for (uint32_t s = 0; s < numSeeds; ++s) {
        tasks.push_back({evalSeeds[s], s, false});
    }
    if (config.evalFlipMap) {
        for (uint32_t s = 0; s < numSeeds; ++s) {
            tasks.push_back({evalSeeds[s], s, true});
        }
    }

    std::vector<ai::MirrorPairResult> resultsOrig(numSeeds);
    std::vector<ai::MirrorPairResult> resultsFlip(config.evalFlipMap ? numSeeds : 0);

    std::atomic<size_t> nextTask{0};
    std::atomic<bool> stopRequested{false};
    g_evalStopRequested = &stopRequested;
    std::signal(SIGINT, sigintHandler);

    auto worker = [&]() {
        while (!stopRequested.load()) {
            size_t idx = nextTask.fetch_add(1);
            if (idx >= tasks.size()) break;
            const auto& t = tasks[idx];
            ai::MirrorPairResult res = ai::evaluatePair(evalParams, opponentParams, t.seed,
                                                        config.maxTicks, nullptr, static_cast<int>(idx * 2 + 1),
                                                        0, &stopRequested, config.stallTicks, t.isFlipped);
            if (!t.isFlipped) {
                resultsOrig[t.resultIdx] = res;
            } else {
                resultsFlip[t.resultIdx] = res;
            }
        }
    };

    std::vector<std::thread> workers;
    uint32_t numThreads = std::max(1u, std::min(config.threadCount, static_cast<uint32_t>(tasks.size())));
    for (uint32_t th = 0; th < numThreads; ++th) {
        workers.emplace_back(worker);
    }
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    g_evalStopRequested = nullptr;

    if (stopRequested.load()) {
        std::cout << "\n[Canh bao] Tien trinh danh gia bi dung som boi nguoi dung (SIGINT).\n";
    }

    EvalMatchStats statsOrig;
    EvalMatchStats statsFlip;
    EvalMatchStats statsCombined;

    for (size_t i = 0; i < numSeeds; ++i) {
        statsOrig.addPair(resultsOrig[i]);
        statsCombined.addPair(resultsOrig[i]);
    }

    if (config.evalFlipMap) {
        for (size_t i = 0; i < numSeeds; ++i) {
            statsFlip.addPair(resultsFlip[i]);
            statsCombined.addPair(resultsFlip[i]);
        }

        statsOrig.print("KET QUA THEO BAN DO: [1] BAN DO GOC (ORIGINAL MAP)");
        statsFlip.print("KET QUA THEO BAN DO: [2] BAN DO LAT GUONG TRUC X (FLIPPED MAP)");
        statsCombined.print("KET QUA TONG HOP (COMBINED - CA 2 BAN DO)", /*allowPooledConclusion=*/false);

        // Tong hop THEO SEED (gop 4 tran cua mot seed lam mot don vi doc lap - M9.14)
        int seedCandidateWins = 0;
        int seedOpponentWins  = 0;
        int seedTies          = 0;

        for (size_t i = 0; i < numSeeds; ++i) {
            int cWins = resultsOrig[i].candidateWins + resultsFlip[i].candidateWins;
            int oWins = resultsOrig[i].opponentWins + resultsFlip[i].opponentWins;
            if (cWins > oWins) {
                seedCandidateWins++;
            } else if (oWins > cWins) {
                seedOpponentWins++;
            } else {
                seedTies++;
            }
        }

        double pValSeed = ai::pairedSignTestPValue(seedCandidateWins, seedOpponentWins);

        std::cout << "\n========================================================================\n"
                  << "KET QUA TONG HOP THEO SEED (GOP 4 TRAN / SEED - DON VI THONG KE DOC LAP)\n"
                  << "========================================================================\n"
                  << "  - Tong so seed danh gia:      " << numSeeds << " seeds (4 tran / seed = " << (numSeeds * 4) << " tran)\n"
                  << "  - Phan bo ket qua theo Seed:\n"
                  << "    * Seed Candidate thang hon (cWins > oWins): " << seedCandidateWins << " (" 
                  << std::fixed << std::setprecision(1) << (numSeeds > 0 ? (seedCandidateWins * 100.0f / numSeeds) : 0.0f) << "%)\n"
                  << "    * Seed Baseline thang hon  (oWins > cWins): " << seedOpponentWins << " (" 
                  << std::fixed << std::setprecision(1) << (numSeeds > 0 ? (seedOpponentWins * 100.0f / numSeeds) : 0.0f) << "%)\n"
                  << "    * Seed Hoa                 (cWins == oWins): " << seedTies << " (" 
                  << std::fixed << std::setprecision(1) << (numSeeds > 0 ? (seedTies * 100.0f / numSeeds) : 0.0f) << "%)\n"
                  << "------------------------------------------------------------------------\n"
                  << "  THONG KE THEO SEED (M9.14):\n"
                  << "  - Sign Test theo Seed (Thang vs Thua): " << seedCandidateWins << " vs " << seedOpponentWins
                  << ", p-value = " << std::setprecision(4) << pValSeed << "\n";

        if (pValSeed < 0.05 && seedCandidateWins > seedOpponentWins) {
            std::cout << "  >>> KET LUAN THEO SEED: TOT HON Baseline (p_seed < 0.05, " << seedCandidateWins << " vs " << seedOpponentWins << " seeds)\n";
        } else if (pValSeed < 0.05 && seedOpponentWins > seedCandidateWins) {
            std::cout << "  >>> KET LUAN THEO SEED: KEM HON Baseline (p_seed < 0.05, " << seedCandidateWins << " vs " << seedOpponentWins << " seeds)\n";
        } else if (seedCandidateWins == seedOpponentWins) {
            std::cout << "  >>> KET LUAN THEO SEED: CAN BANG voi Baseline (" << seedCandidateWins << " vs " << seedOpponentWins << " seeds)\n";
        } else {
            std::cout << "  >>> KET LUAN THEO SEED: CHUA KET LUAN (p_seed >= 0.05, can them seed de khang dinh)\n";
        }

        std::cout << "  - Chú thích: Bản đồ gốc và bản đồ lật dùng chung seed nên không độc lập;\n"
                  << "    kết luận thống kê chuẩn xác cần dựa trên đơn vị Seed (gom 4 trận / seed).\n"
                  << "========================================================================\n";
    } else {
        statsOrig.print("KET QUA DANH GIA PROFILE VS BASELINE");
    }

    std::cout << "\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    enableWindowsAnsi();

    ai::TrainerConfig config;
    uint32_t expectedProcs = 1;
    bool threadsExplicitlySet = false;

    std::string evalOnlyFile;
    bool vsBaseline = false;
    uint32_t evalSeedCount = 8;
    uint64_t seedBase = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "--help" || arg == "-h")) {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--eval-only" && i + 1 < argc) {
            evalOnlyFile = argv[++i];
        } else if (arg == "--vs-baseline") {
            vsBaseline = true;
        } else if (arg == "--eval-flip-map") {
            config.evalFlipMap = true;
        } else if (arg == "--seeds" && i + 1 < argc) {
            evalSeedCount = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed-base" && i + 1 < argc) {
            seedBase = std::stoull(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            config.mode = ai::stringToTrainingMode(argv[++i]);
        } else if (arg == "--generations" && i + 1 < argc) {
            config.generations = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--population" && i + 1 < argc) {
            config.populationSize = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seeds-per-gen" && i + 1 < argc) {
            config.seedsPerGen = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--validation-seeds" && i + 1 < argc) {
            config.validationSeeds = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-ticks" && i + 1 < argc) {
            config.maxTicks = std::stoull(argv[++i]);
        } else if (arg == "--stall-ticks" && i + 1 < argc) {
            config.stallTicks = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--threads" && i + 1 < argc) {
            config.threadCount = static_cast<uint32_t>(std::stoul(argv[++i]));
            threadsExplicitlySet = true;
        } else if ((arg == "--procs" || arg == "--expected-procs") && i + 1 < argc) {
            expectedProcs = std::max(1u, static_cast<uint32_t>(std::stoul(argv[++i])));
        } else if (arg == "--seed" && i + 1 < argc) {
            config.masterSeed = std::stoull(argv[++i]);
        } else if (arg == "--resume" && i + 1 < argc) {
            config.resumeFile = argv[++i];
        } else if (arg == "--baseline" && i + 1 < argc) {
            config.baselineFile = argv[++i];
        } else if (arg == "--out-best" && i + 1 < argc) {
            config.outBestFile = argv[++i];
        } else if (arg == "--log" && i + 1 < argc) {
            config.logCsvFile = argv[++i];
        } else if (arg == "--save-showcase-replay" && i + 1 < argc) {
            config.showcaseReplayFile = argv[++i];
        } else if (arg == "--print-every" && i + 1 < argc) {
            config.printEvery = std::stoi(argv[++i]);
        } else if (arg == "--live") {
            config.liveOutput = true;
            config.scrollOutput = false;
            config.quietOutput = false;
        } else if (arg == "--scroll") {
            config.liveOutput = false;
            config.scrollOutput = true;
            config.quietOutput = false;
        } else if (arg == "--quiet") {
            config.liveOutput = false;
            config.scrollOutput = false;
            config.quietOutput = true;
        } else {
            std::cerr << "[War_Strategy_Train] Khong nhan dien duoc tuy chon: " << arg << " (dung --help de xem)\n";
            return 1;
        }
    }

    if (!threadsExplicitlySet) {
        unsigned int hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        config.threadCount = std::max(1u, hw / expectedProcs);
    }

    if (!evalOnlyFile.empty()) {
        return runEvaluationOnly(evalOnlyFile, vsBaseline, evalSeedCount, config, seedBase);
    }

    if (std::filesystem::exists(config.outBestFile) && config.resumeFile.empty()) {
        std::cerr << "[CANH BAO] File --out-best '" << config.outBestFile
                  << "' da ton tai va khong co --resume! Khi chay song song, bat buoc dung --out-best khac nhau de tranh ghi de.\n";
    }

    ai::AiParams baselineParams{};
    if (!config.baselineFile.empty()) {
        if (!ai::loadTrainedAiProfile(config.baselineFile, baselineParams)) {
            std::cerr << "[CANH BAO] Khong the nap baseline profile: " << config.baselineFile
                      << ", se dung mac dinh AiParams{}.\n";
        }
    }

    std::cout << "========================================================================\n"
              << "War Strategy - Module Huan Luyen AI Tu Hoc (Milestone 9)\n"
              << "========================================================================\n"
              << "  - Mode:           " << ai::trainingModeToString(config.mode) << "\n";
    printBaselineInfo(config.baselineFile, baselineParams);
    std::cout << "  - Generations:    " << config.generations << "\n"
              << "  - Population:     " << config.populationSize << "\n"
              << "  - Seeds/Gen:      " << config.seedsPerGen << "\n"
              << "  - Validation Seeds:" << config.validationSeeds << "\n"
              << "  - Max Ticks:      " << config.maxTicks << " (~" << (config.maxTicks / 20) << "s in-game)\n"
              << "  - Threads:        " << config.threadCount << "\n"
              << "  - Master Seed:    " << config.masterSeed << "\n"
              << "  - Resume Profile: " << (config.resumeFile.empty() ? "(None)" : config.resumeFile) << "\n"
              << "  - Out Best:       " << config.outBestFile << "\n"
              << "  - Log CSV:        " << (config.logCsvFile.empty() ? "(None)" : config.logCsvFile) << "\n"
              << "  - Showcase Replay:" << (config.showcaseReplayFile.empty() ? "(None)" : config.showcaseReplayFile) << "\n"
              << "========================================================================\n\n";

    ai::AiTrainer trainer(config);
    g_activeTrainer = &trainer;
    std::signal(SIGINT, sigintHandler);

    bool success = trainer.train();
    g_activeTrainer = nullptr;

    return success ? 0 : 1;
}
