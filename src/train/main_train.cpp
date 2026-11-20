#include "../ai/AiTrainer.hpp"
#include "../ai/AiTelemetry.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <thread>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

ai::AiTrainer* g_activeTrainer = nullptr;

void sigintHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\n[Ctrl+C] Nhan tin hieu ngat (SIGINT)! Dang dung muot va luu ket qua tot nhat...\n" << std::flush;
        if (g_activeTrainer) {
            g_activeTrainer->requestStop();
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
              << "Options:\n"
              << "  --mode <evo|adapt>           Thuat toan huan luyen: evo (tien hoa quan the) hoac adapt (thich nghi sau tran thua)\n"
              << "                               (Mac dinh: evo)\n"
              << "  --generations <N>            So the he tien hoa hoac chu ky thich nghi (Mac dinh: 10)\n"
              << "  --population <N>             Kich thuoc quan the moi the he (Mac dinh: 8)\n"
              << "  --seeds-per-gen <N>          So seed ban do moi the he (Mac dinh: 2, tuc moi cap la 4 tran mirror)\n"
              << "  --max-ticks <N>              Gioi han tick toi da moi tran (Mac dinh: 6000 = 5 phut in-game)\n"
              << "  --threads <N>                So luong CPU chay song song (Mac dinh: hardware_concurrency)\n"
              << "  --seed <uint64>              Master seed tat dinh cho toan bo qua trinh train (Mac dinh: 12345)\n"
              << "  --resume <file.json>         Tiep tuc huan luyen tu mot file profile AiParams da luu\n"
              << "  --out-best <file.json>       Duong dan luu profile bo tham so manh nhat (Mac dinh: data/ai_best_params.json)\n"
              << "  --log <file.csv>             Ghi log CSV lich su chi tiet tung the he / chu ky\n"
              << "  --save-showcase-replay <f>   Sau khi train xong, chay 1 tran Champion AI vs Baseline va luu replay JSON\n"
              << "  --live                       Che do cap nhat truc quan tai cho tren CMD qua ANSI (Mac dinh neu la TTY)\n"
              << "  --scroll                     Che do in cuon tung cap dong kem trang thai\n"
              << "  --quiet                      Tat hien thi telemetry truc quan, chi in tong ket\n"
              << "  --print-every <ticks>        Tan suat lay mau suy nghi cua AI tu Worker 0 (Mac dinh: 20 ticks)\n"
              << "  --help, -h                   Hien thi huong dan su dung nay\n"
              << "========================================================================\n";
}

} // namespace

int main(int argc, char* argv[]) {
    enableWindowsAnsi();

    ai::TrainerConfig config;
    config.threadCount = std::thread::hardware_concurrency();
    if (config.threadCount == 0) config.threadCount = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "--help" || arg == "-h")) {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--mode" && i + 1 < argc) {
            config.mode = ai::stringToTrainingMode(argv[++i]);
        } else if (arg == "--generations" && i + 1 < argc) {
            config.generations = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--population" && i + 1 < argc) {
            config.populationSize = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seeds-per-gen" && i + 1 < argc) {
            config.seedsPerGen = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-ticks" && i + 1 < argc) {
            config.maxTicks = std::stoull(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            config.threadCount = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            config.masterSeed = std::stoull(argv[++i]);
        } else if (arg == "--resume" && i + 1 < argc) {
            config.resumeFile = argv[++i];
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

    std::cout << "========================================================================\n"
              << "War Strategy - Module Huan Luyen AI Tu Hoc (Milestone 9)\n"
              << "========================================================================\n"
              << "  - Mode:           " << ai::trainingModeToString(config.mode) << "\n"
              << "  - Generations:    " << config.generations << "\n"
              << "  - Population:     " << config.populationSize << "\n"
              << "  - Seeds/Gen:      " << config.seedsPerGen << "\n"
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
