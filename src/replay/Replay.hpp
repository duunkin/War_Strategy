#pragma once

#include "../sim/commands/Command.hpp"
#include "../sim/combat/CombatSystem.hpp"
#include "../ai/AiParams.hpp"
#include "../ai/AiTelemetry.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace sim {
class WorldSim;
}

namespace replay {

/**
 * @brief Bản ghi một lệnh tại một thời điểm tick xác định
 */
struct ReplayRecord {
    uint64_t     tick{0};
    sim::Command command;
};

/**
 * @brief Header thông tin của file Replay
 */
struct ReplayHeader {
    uint32_t         formatVersion{1};
    uint64_t         seed{0};
    uint64_t         maxTicks{72000};
    ai::AiParams     ai1Params;
    ai::AiParams     ai2Params;
    sim::MatchResult result{sim::MatchResult::InProgress};
    std::string      endReason;
    uint64_t         finalTick{0};
};

/**
 * @brief Class ReplayRecorder: ghi lại toàn bộ Command đi vào WorldSim theo từng tick và lưu xuống đĩa
 */
class ReplayRecorder {
public:
    ReplayRecorder() = default;
    ~ReplayRecorder() = default;

    // Bắt đầu phiên ghi replay
    void start(uint64_t seed, uint64_t maxTicks,
               const ai::AiParams& ai1Params, const ai::AiParams& ai2Params);

    // Ghi một command tại tick
    void recordCommand(uint64_t tick, const sim::Command& cmd);

    // Đánh dấu kết thúc trận đấu và lưu metadata
    void finish(sim::MatchResult result, const std::string& reason, uint64_t finalTick);

    // Lưu toàn bộ dữ liệu replay ra file (định dạng JSON)
    [[nodiscard]] bool saveToFile(const std::string& filepath) const;

    // Truy cập dữ liệu nội bộ
    [[nodiscard]] const ReplayHeader& getHeader() const { return m_header; }
    [[nodiscard]] const std::vector<ReplayRecord>& getRecords() const { return m_records; }
    [[nodiscard]] size_t getRecordCount() const { return m_records.size(); }

    void clear();

private:
    ReplayHeader              m_header;
    std::vector<ReplayRecord> m_records;
    bool                      m_isRecording{false};
};

/**
 * @brief Class ReplayPlayer: nạp file replay từ đĩa và bơm đúng các Command vào WorldSim tại đúng tick
 */
class ReplayPlayer {
public:
    ReplayPlayer() = default;
    ~ReplayPlayer() = default;

    // Nạp file replay từ đường dẫn
    [[nodiscard]] bool loadFromFile(const std::string& filepath);

    // Nạp trực tiếp từ header và records (dùng trong test hoặc memory playback)
    void loadFromData(const ReplayHeader& header, const std::vector<ReplayRecord>& records);

    // Áp dụng các command của tick hiện tại vào WorldSim
    void applyCommandsForTick(uint64_t tick, sim::WorldSim& sim);

    // Đặt lại con trỏ phát lại từ đầu
    void resetPlayback();

    [[nodiscard]] bool isFinished(uint64_t currentTick) const;
    [[nodiscard]] const ReplayHeader& getHeader() const { return m_header; }
    [[nodiscard]] const std::vector<ReplayRecord>& getRecords() const { return m_records; }
    [[nodiscard]] uint64_t getSeed() const { return m_header.seed; }
    [[nodiscard]] uint64_t getMaxTicks() const { return m_header.maxTicks; }
    [[nodiscard]] uint64_t getFinalTick() const { return m_header.finalTick; }
    [[nodiscard]] size_t getRecordCount() const { return m_records.size(); }

private:
    ReplayHeader              m_header;
    std::vector<ReplayRecord> m_records;
    size_t                    m_playbackIndex{0};
    bool                      m_isLoaded{false};
};

} // namespace replay
