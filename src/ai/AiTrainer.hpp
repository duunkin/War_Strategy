#pragma once

#include "AiParams.hpp"
#include "AiTelemetry.hpp"
#include "../sim/WorldSim.hpp"
#include "../mapgen/FastRng.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <atomic>
#include <mutex>
#include <memory>

namespace ai {

/**
 * @brief Chế độ huấn luyện AI (M9)
 */
enum class TrainingMode : uint8_t {
    Evolution = 0,  // Tiến hóa quần thể (Genetic Algorithm / Self-Play)
    Adaptation      // Tự học thích nghi sau mỗi trận thua (Post-Loss Adaptation)
};

inline const char* trainingModeToString(TrainingMode m) {
    switch (m) {
        case TrainingMode::Evolution:  return "evo";
        case TrainingMode::Adaptation: return "adapt";
        default:                       return "unknown";
    }
}

inline TrainingMode stringToTrainingMode(const std::string& str) {
    if (str == "adapt" || str == "adaptation") return TrainingMode::Adaptation;
    return TrainingMode::Evolution;
}

/**
 * @brief Kết quả một trận đấu đơn lập giữa 2 AI
 */
struct SingleMatchResult {
    bool              candidateIsPlayer{true}; // true: candidate cầm Player (Blue); false: candidate cầm Enemy (Red)
    sim::MatchResult  result{sim::MatchResult::InProgress};
    sim::MatchEndKind endKind{sim::MatchEndKind::Draw};
    std::string       endReason;
    uint64_t          finalTick{0};

    int              candidateAliveUnits{0};
    int              candidateTotalTroops{0};
    int              candidateCommanderHp{0};

    int              opponentAliveUnits{0};
    int              opponentTotalTroops{0};
    int              opponentCommanderHp{0};

    float            fitnessScore{0.0f};
    bool             isWin{false};
    bool             isLoss{false};
    bool             isDraw{false};
};

/**
 * @brief Kết quả cặp trận đấu công bằng Mirror Match (đổi sân Blue/Red trên cùng 1 seed)
 */
struct MirrorPairResult {
    uint64_t          seed{0};
    SingleMatchResult matchA; // Candidate cầm Player (Blue) vs Opponent cầm Enemy (Red)
    SingleMatchResult matchB; // Opponent cầm Player (Blue) vs Candidate cầm Enemy (Red)

    float             totalFitness{0.0f};
    int               candidateWins{0};
    int               opponentWins{0};
    int               draws{0};

    [[nodiscard]] float getWinRate() const {
        return (candidateWins + opponentWins + draws > 0)
            ? (static_cast<float>(candidateWins) / static_cast<float>(candidateWins + opponentWins + draws))
            : 0.0f;
    }
};

/**
 * @brief Chẩn đoán nguyên nhân thua trận cho chế độ thích nghi (Post-Loss Adaptation)
 */
struct MatchDiagnosis {
    enum class LossCategory : uint8_t {
        CommanderAssassinated = 0, // Tư lệnh bị tiêu diệt
        TroopAttrition,            // Mất quân quá nhanh / bị tiêu hao áp đảo
        OutgunnedOrTimeout,        // Hết giờ ít quân hơn hoặc bị áp đảo hỏa lực
        None
    };

    LossCategory category{LossCategory::None};
    int          friendlyTroopLoss{0};
    int          enemyTroopLoss{0};
    int          finalCommanderHp{0};
    bool         commanderDied{false};
    uint64_t     ticksElapsed{0};
    std::string  reason;
};

/**
 * @brief Cấu hình phiên huấn luyện AI (M9)
 */
struct TrainerConfig {
    TrainingMode mode{TrainingMode::Evolution};

    uint32_t     generations{10};      // Số thế hệ (evo) hoặc số chu kỳ thích nghi (adapt)
    uint32_t     populationSize{8};    // Kích thước quần thể (evo)
    uint32_t     seedsPerGen{6};       // Số seed bản đồ mỗi thế hệ (mỗi seed chạy 1 cặp mirror = 2 trận)
    uint64_t     maxTicks{24000};      // Giới hạn tick mỗi trận (24000 tick = 20 phút in-game)
    uint32_t     threadCount{1};       // Số luồng CPU chạy song song
    uint64_t     masterSeed{12345ULL}; // Master seed tất định
    uint32_t     validationSeeds{8};   // Số seed validation cố định đánh giá top-3

    uint32_t     eliteCount{2};        // Số cá thể tinh hoa giữ nguyên qua từng thế hệ
    float        mutationRateInitial{0.35f}; // Tỉ lệ đột biến ban đầu
    float        mutationRateFinal{0.12f};   // Tỉ lệ đột biến cuối kỳ (annealing)

    std::string  resumeFile;           // Nạp profile AI đã huấn luyện trước đó
    std::string  outBestFile{"data/ai_best_params.json"}; // Đường dẫn lưu profile vô địch
    std::string  logCsvFile;           // Đường dẫn lưu file log CSV
    std::string  showcaseReplayFile;   // Đường dẫn lưu replay trận biểu diễn Champion vs Baseline

    int          printEvery{20};       // Tần suất cập nhật telemetry (ticks)
    bool         liveOutput{true};     // Cập nhật tại chỗ trên CMD
    bool         scrollOutput{false};  // Cuộn log trên CMD
    bool         quietOutput{false};   // Tắt telemetry, chỉ in tóm tắt
};

/**
 * @brief Cá thể trong quần thể tiến hóa
 */
struct Individual {
    AiParams params;
    float    fitness{0.0f};
    float    winRateVsBaseline{0.0f};
    int      matchesWon{0};
    int      totalMatches{0};
};

/**
 * @brief Snapshot theo dõi thread-safe phục vụ hiển thị CMD Worker 0
 */
struct LiveTelemetryWatch {
    std::mutex        mutex;
    uint32_t          currentGen{0};
    uint32_t          totalGen{0};
    uint32_t          currentMatch{0};
    uint32_t          totalMatches{0};
    float             bestFitness{-99999.0f};
    float             winRateVsBaseline{0.0f};
    double            ticksPerSecond{0.0};
    double            realtimeMultiplier{0.0};

    bool              hasActiveSnapshot{false};
    uint64_t          simTick{0};
    int               matchIndex{0};
    AiThoughtSnapshot snapAI1;
    AiThoughtSnapshot snapAI2;
};

// =============================================================================
// Hàm tính điểm Fitness & Chẩn đoán độc lập (Thread-Safe)
// =============================================================================

/**
 * @brief Tính điểm fitness liên tục có độ phân giải cao cho 1 trận đấu
 */
float calculateSingleMatchFitness(const SingleMatchResult& res, uint64_t maxTicks);

/**
 * @brief Chẩn đoán nguyên nhân thất bại của một phe trong trận đấu (dùng thông số trận thua)
 */
MatchDiagnosis diagnoseMatch(const SingleMatchResult& lossMatch);
MatchDiagnosis diagnoseMatch(const sim::WorldSim& sim, sim::Faction loserFaction, const std::string& endReason);

/**
 * @brief Tinh chỉnh tham số có định hướng dựa trên chẩn đoán thất bại (M9 Post-Loss Adaptation)
 */
AiParams adaptParamsAfterLoss(const AiParams& loser, const MatchDiagnosis& diag, mapgen::FastRng& rng);

/**
 * @brief Chạy 1 trận đấu mô phỏng giữa 2 bộ tham số trên seed xác định (Thread-Safe)
 */
SingleMatchResult runSimulationMatch(const AiParams& playerParams,
                                     const AiParams& enemyParams,
                                     bool candidateIsPlayer,
                                     uint64_t seed,
                                     uint64_t maxTicks,
                                     LiveTelemetryWatch* liveWatch = nullptr,
                                     int matchIndex = 0,
                                     int printEvery = 20,
                                     const std::atomic<bool>* stopRequested = nullptr);

/**
 * @brief Chạy 1 cặp Mirror Match (Trận A: Cand Blue vs Opp Red; Trận B: Opp Blue vs Cand Red)
 */
MirrorPairResult evaluatePair(const AiParams& candidate,
                              const AiParams& opponent,
                              uint64_t seed,
                              uint64_t maxTicks,
                              LiveTelemetryWatch* liveWatch = nullptr,
                              int matchBaseIndex = 0,
                              int printEvery = 20,
                              const std::atomic<bool>* stopRequested = nullptr);

// =============================================================================
// Class AiTrainer: Điều phối toàn bộ quá trình huấn luyện đa luồng
// =============================================================================
class AiTrainer {
public:
    explicit AiTrainer(const TrainerConfig& config);
    ~AiTrainer() = default;

    // Chạy huấn luyện (tự động phân nhánh theo config.mode)
    bool train();

    // Yêu cầu dừng mượt khi nhận Ctrl+C (SIGINT)
    void requestStop();

    [[nodiscard]] const AiParams& getBestParams() const { return m_bestParams; }
    [[nodiscard]] const AiTrainingMetadata& getBestMetadata() const { return m_bestMetadata; }
    [[nodiscard]] bool isStopRequested() const { return m_stopRequested.load(); }

private:
    bool runEvolution();
    bool runAdaptation();

    // Sinh showcase replay giữa Champion AI và Baseline
    bool generateShowcaseReplay(const AiParams& championParams, uint64_t seed);

    // Lai ghép & đột biến cá thể
    AiParams crossover(const AiParams& p1, const AiParams& p2, mapgen::FastRng& rng);
    AiParams mutate(const AiParams& p, float mutationRate, mapgen::FastRng& rng);

    // Ghi header CSV
    void initCsvLog();
    // Ghi dòng log CSV cho 1 thế hệ
    void logCsvGeneration(uint32_t gen, float bestFit, float avgFit, float winRateVsBase,
                          int matches, double wallMs, const AiParams& bestP);

private:
    TrainerConfig       m_config;
    std::atomic<bool>   m_stopRequested{false};
    AiParams            m_bestParams;
    AiTrainingMetadata  m_bestMetadata;
    LiveTelemetryWatch  m_liveWatch;
};

} // namespace ai
