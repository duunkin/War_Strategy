#include "Replay.hpp"
#include "../sim/WorldSim.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace replay {

namespace {

std::string matchResultToString(sim::MatchResult res) {
    switch (res) {
        case sim::MatchResult::Victory:    return "Victory";
        case sim::MatchResult::Defeat:     return "Defeat";
        case sim::MatchResult::Draw:       return "Draw";
        case sim::MatchResult::InProgress: return "InProgress";
        default:                           return "Unknown";
    }
}

sim::MatchResult stringToMatchResult(const std::string& str) {
    if (str == "Victory")    return sim::MatchResult::Victory;
    if (str == "Defeat")     return sim::MatchResult::Defeat;
    if (str == "Draw")       return sim::MatchResult::Draw;
    return sim::MatchResult::InProgress;
}

} // namespace

void ReplayRecorder::start(uint64_t seed, uint64_t maxTicks,
                           const ai::AiParams& ai1Params, const ai::AiParams& ai2Params) {
    m_header.formatVersion = 1;
    m_header.seed = seed;
    m_header.maxTicks = maxTicks;
    m_header.ai1Params = ai1Params;
    m_header.ai2Params = ai2Params;
    m_header.result = sim::MatchResult::InProgress;
    m_header.endReason.clear();
    m_header.finalTick = 0;
    m_records.clear();
    m_isRecording = true;
}

void ReplayRecorder::recordCommand(uint64_t tick, const sim::Command& cmd) {
    if (!m_isRecording) return;
    m_records.push_back(ReplayRecord{tick, cmd});
}

void ReplayRecorder::finish(sim::MatchResult result, const std::string& reason, uint64_t finalTick) {
    m_header.result = result;
    m_header.endReason = reason;
    m_header.finalTick = finalTick;
    m_isRecording = false;
}

void ReplayRecorder::clear() {
    m_header = ReplayHeader{};
    m_records.clear();
    m_isRecording = false;
}

bool ReplayRecorder::saveToFile(const std::string& filepath) const {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json root;
        root["formatVersion"] = m_header.formatVersion;
        root["seed"] = m_header.seed;
        root["maxTicks"] = m_header.maxTicks;
        root["finalTick"] = m_header.finalTick;
        root["result"] = matchResultToString(m_header.result);
        root["endReason"] = m_header.endReason;

        nlohmann::json ai1J;
        ai::to_json(ai1J, m_header.ai1Params);
        root["ai1Params"] = ai1J;

        nlohmann::json ai2J;
        ai::to_json(ai2J, m_header.ai2Params);
        root["ai2Params"] = ai2J;

        nlohmann::json cmdArray = nlohmann::json::array();
        for (const auto& rec : m_records) {
            nlohmann::json c;
            c["tick"] = rec.tick;
            c["type"] = (rec.command.type == sim::CommandType::Move) ? "Move" : "Unknown";
            c["unitIds"] = rec.command.move.unitIds;
            c["targetX"] = rec.command.move.targetTileX;
            c["targetY"] = rec.command.move.targetTileY;
            cmdArray.push_back(std::move(c));
        }
        root["commands"] = std::move(cmdArray);

        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[ReplayRecorder] Khong the tao file: " << filepath << std::endl;
            return false;
        }

        file << root.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ReplayRecorder] Exception khi ghi file replay: " << e.what() << std::endl;
        return false;
    }
}

bool ReplayPlayer::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[ReplayPlayer] Khong the mo file replay: " << filepath << std::endl;
            return false;
        }

        nlohmann::json root = nlohmann::json::parse(file);
        m_header.formatVersion = root.value("formatVersion", 1U);
        m_header.seed = root.value("seed", 0ULL);
        m_header.maxTicks = root.value("maxTicks", 72000ULL);
        m_header.finalTick = root.value("finalTick", 0ULL);
        m_header.result = stringToMatchResult(root.value("result", "InProgress"));
        m_header.endReason = root.value("endReason", "");

        if (root.contains("ai1Params")) {
            ai::from_json(root["ai1Params"], m_header.ai1Params);
        }
        if (root.contains("ai2Params")) {
            ai::from_json(root["ai2Params"], m_header.ai2Params);
        }

        m_records.clear();
        if (root.contains("commands") && root["commands"].is_array()) {
            for (const auto& item : root["commands"]) {
                ReplayRecord rec;
                rec.tick = item.value("tick", 0ULL);
                std::string typeStr = item.value("type", "Move");
                if (typeStr == "Move") {
                    rec.command.type = sim::CommandType::Move;
                }
                if (item.contains("unitIds") && item["unitIds"].is_array()) {
                    rec.command.move.unitIds = item["unitIds"].get<std::vector<uint32_t>>();
                }
                rec.command.move.targetTileX = item.value("targetX", 0);
                rec.command.move.targetTileY = item.value("targetY", 0);
                m_records.push_back(rec);
            }
        }

        // Đảm bảo records được sắp xếp theo thứ tự tick
        std::stable_sort(m_records.begin(), m_records.end(), [](const ReplayRecord& a, const ReplayRecord& b) {
            return a.tick < b.tick;
        });

        m_playbackIndex = 0;
        m_isLoaded = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ReplayPlayer] Exception khi doc file replay: " << e.what() << std::endl;
        m_isLoaded = false;
        return false;
    }
}

void ReplayPlayer::loadFromData(const ReplayHeader& header, const std::vector<ReplayRecord>& records) {
    m_header = header;
    m_records = records;
    std::stable_sort(m_records.begin(), m_records.end(), [](const ReplayRecord& a, const ReplayRecord& b) {
        return a.tick < b.tick;
    });
    m_playbackIndex = 0;
    m_isLoaded = true;
}

void ReplayPlayer::resetPlayback() {
    m_playbackIndex = 0;
}

void ReplayPlayer::applyCommandsForTick(uint64_t tick, sim::WorldSim& sim) {
    if (!m_isLoaded) return;
    while (m_playbackIndex < m_records.size() && m_records[m_playbackIndex].tick == tick) {
        sim.queueCommand(m_records[m_playbackIndex].command);
        ++m_playbackIndex;
    }
}

bool ReplayPlayer::isFinished(uint64_t currentTick) const {
    if (!m_isLoaded) return true;
    if (m_header.finalTick > 0 && currentTick >= m_header.finalTick) {
        return true;
    }
    return (m_playbackIndex >= m_records.size() && currentTick >= m_header.maxTicks);
}

} // namespace replay
