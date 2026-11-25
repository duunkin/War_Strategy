#pragma once

#include "AiParams.hpp"
#include "../sim/units/Unit.hpp"

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace ai {

/**
 * @brief Trạng thái tư duy chiến thuật tổng thể của AI
 */
enum class AiPosture : uint8_t {
    Scouting = 0,   // Trinh sát mở sương mù, thăm dò chiến trường
    Advancing,      // Tiến công theo đội hình, chiếm giữ các vị trí xung yếu
    Engaging,       // Đang giao tranh trực tiếp, tập trung hỏa lực
    DefendCmd,      // Tư lệnh gặp nguy, ưu tiên bảo vệ / hộ tống tư lệnh
    Retreating,     // Rút lui chiến thuật bảo toàn lực lượng
    AllOut          // Tổng lực áp đảo truy kích
};

/**
 * @brief Chuyển đổi AiPosture sang chuỗi hiển thị gọn trên CMD
 */
inline const char* postureToString(AiPosture p) {
    switch (p) {
        case AiPosture::Scouting:   return "SCOUTING";
        case AiPosture::Advancing:  return "ADVANCING";
        case AiPosture::Engaging:   return "ENGAGING";
        case AiPosture::DefendCmd:  return "DEFEND_CMD";
        case AiPosture::Retreating: return "RETREATING";
        case AiPosture::AllOut:     return "ALL_OUT";
        default:                    return "UNKNOWN";
    }
}

inline AiPosture stringToPosture(const std::string& str) {
    if (str == "SCOUTING")   return AiPosture::Scouting;
    if (str == "ADVANCING")  return AiPosture::Advancing;
    if (str == "ENGAGING")   return AiPosture::Engaging;
    if (str == "DEFEND_CMD") return AiPosture::DefendCmd;
    if (str == "RETREATING") return AiPosture::Retreating;
    if (str == "ALL_OUT")    return AiPosture::AllOut;
    return AiPosture::Scouting;
}

/**
 * @brief Snapshot tư duy của AI tại một nhịp ra quyết định (M8 Telemetry)
 *
 * Gọn nhẹ, không cấp phát thừa, lưu giữ trạng thái phản ánh trực tiếp quá trình suy nghĩ.
 */
struct AiThoughtSnapshot {
    sim::Faction faction{sim::Faction::Neutral};
    uint64_t     decisionTick{0};
    AiPosture    posture{AiPosture::Scouting};
    std::string  primaryIntent{"Init"}; // Ý đồ nổi bật: CmdFlee, ArtyStrike, AirHunt, B2Raid, NavalSupport, FlankPush, HoldForest...

    // Thông tin tình báo sương mù chiến tranh
    int visibleEnemies{0};      // Số đơn vị địch đang thấy trực tiếp qua FoW
    int rememberedEnemies{0};   // Số đơn vị địch đang nhớ trong bộ nhớ tình báo

    // Phân bổ vai trò quân phe mình
    int aliveUnits{0};          // Tổng số đơn vị sống
    int totalTroops{0};         // Tổng quân số sống
    int commanderHp{0};         // Máu tư lệnh hiện tại
    int commanderMaxHp{100};    // Máu tư lệnh tối đa
    bool commanderInDanger{false};

    int attackingUnits{0};      // Số đơn vị đang tiến công / giao chiến
    int scoutingUnits{0};       // Số đơn vị đang trinh sát
    int retreatingUnits{0};     // Số đơn vị đang tháo lui hồi phục

    // Thống kê lệnh phát ra
    uint32_t commandsIssuedThisCycle{0};
    uint32_t totalCommandsIssued{0};

    // Tham số AI đang áp dụng
    AiParams params;
};

// =============================================================================
// Tiện ích Serialize / Deserialize AiParams với nlohmann::json
// =============================================================================
inline void to_json(nlohmann::json& j, const AiParams& p) {
    j = nlohmann::json{
        {"decisionIntervalTicks", p.decisionIntervalTicks},
        {"aggression", p.aggression},
        {"retreatHpRatio", p.retreatHpRatio},
        {"commanderSafeRadiusTiles", p.commanderSafeRadiusTiles},
        {"artilleryStandOffMargin", p.artilleryStandOffMargin},
        {"artilleryIdealRangeRatio", p.artilleryIdealRangeRatio},
        {"scoutingWeight", p.scoutingWeight},
        {"focusFireWeight", p.focusFireWeight},
        {"targetMemoryTicks", p.targetMemoryTicks},
        {"enableCommanderRetreat", p.enableCommanderRetreat},
        {"enableUnitRetreat", p.enableUnitRetreat},
        {"scoutingGroupCount", p.scoutingGroupCount},
        {"threatRadiusTiles", p.threatRadiusTiles}
    };
}

inline void from_json(const nlohmann::json& j, AiParams& p) {
    if (j.contains("decisionIntervalTicks")) j.at("decisionIntervalTicks").get_to(p.decisionIntervalTicks);
    if (j.contains("aggression")) j.at("aggression").get_to(p.aggression);
    if (j.contains("retreatHpRatio")) j.at("retreatHpRatio").get_to(p.retreatHpRatio);
    if (j.contains("commanderSafeRadiusTiles")) j.at("commanderSafeRadiusTiles").get_to(p.commanderSafeRadiusTiles);
    if (j.contains("artilleryStandOffMargin")) j.at("artilleryStandOffMargin").get_to(p.artilleryStandOffMargin);
    if (j.contains("artilleryIdealRangeRatio")) j.at("artilleryIdealRangeRatio").get_to(p.artilleryIdealRangeRatio);
    if (j.contains("scoutingWeight")) j.at("scoutingWeight").get_to(p.scoutingWeight);
    if (j.contains("focusFireWeight")) j.at("focusFireWeight").get_to(p.focusFireWeight);
    if (j.contains("targetMemoryTicks")) j.at("targetMemoryTicks").get_to(p.targetMemoryTicks);
    if (j.contains("enableCommanderRetreat")) j.at("enableCommanderRetreat").get_to(p.enableCommanderRetreat);
    if (j.contains("enableUnitRetreat")) j.at("enableUnitRetreat").get_to(p.enableUnitRetreat);
    if (j.contains("scoutingGroupCount")) j.at("scoutingGroupCount").get_to(p.scoutingGroupCount);
    if (j.contains("threatRadiusTiles")) {
        j.at("threatRadiusTiles").get_to(p.threatRadiusTiles);
    } else {
        p.threatRadiusTiles = 16.0f;
    }
    clampParams(p);
}

// =============================================================================
// Metadata huấn luyện AI (M9)
// =============================================================================
struct AiTrainingMetadata {
    uint32_t    generation{0};
    float       fitness{0.0f};
    float       winRateVsBaseline{0.0f};
    uint32_t    trainedMatches{0};
    std::string algorithm{"evo"}; // "evo" | "adapt"
    std::string timestamp{""};
    std::string baselineProfile{"defaults"}; // M9.12
};

inline void to_json(nlohmann::json& j, const AiTrainingMetadata& m) {
    j = nlohmann::json{
        {"generation", m.generation},
        {"fitness", m.fitness},
        {"winRateVsBaseline", m.winRateVsBaseline},
        {"trainedMatches", m.trainedMatches},
        {"algorithm", m.algorithm},
        {"timestamp", m.timestamp},
        {"baselineProfile", m.baselineProfile}
    };
}

inline void from_json(const nlohmann::json& j, AiTrainingMetadata& m) {
    if (j.contains("generation")) j.at("generation").get_to(m.generation);
    if (j.contains("fitness")) j.at("fitness").get_to(m.fitness);
    if (j.contains("winRateVsBaseline")) j.at("winRateVsBaseline").get_to(m.winRateVsBaseline);
    if (j.contains("trainedMatches")) j.at("trainedMatches").get_to(m.trainedMatches);
    if (j.contains("algorithm")) j.at("algorithm").get_to(m.algorithm);
    if (j.contains("timestamp")) j.at("timestamp").get_to(m.timestamp);
    if (j.contains("baselineProfile")) {
        j.at("baselineProfile").get_to(m.baselineProfile);
    } else {
        m.baselineProfile = "defaults";
    }
}

// =============================================================================
// So sánh tham số AiParams với bộ mặc định (M9.12)
// =============================================================================
struct ParamDiff {
    std::string name;
    std::string defaultValue;
    std::string currentValue;
};

inline std::vector<ParamDiff> getParamDiffsFromDefault(const AiParams& p) {
    AiParams d{};
    std::vector<ParamDiff> diffs;
    auto formatFloat = [](float val) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << val;
        std::string str = ss.str();
        str.erase(str.find_last_not_of('0') + 1, std::string::npos);
        if (str.back() == '.') str.push_back('0');
        return str;
    };

    if (p.decisionIntervalTicks != d.decisionIntervalTicks) {
        diffs.push_back({"decisionIntervalTicks", std::to_string(d.decisionIntervalTicks), std::to_string(p.decisionIntervalTicks)});
    }
    if (std::abs(p.aggression - d.aggression) >= 1e-4f) {
        diffs.push_back({"aggression", formatFloat(d.aggression), formatFloat(p.aggression)});
    }
    if (std::abs(p.retreatHpRatio - d.retreatHpRatio) >= 1e-4f) {
        diffs.push_back({"retreatHpRatio", formatFloat(d.retreatHpRatio), formatFloat(p.retreatHpRatio)});
    }
    if (std::abs(p.commanderSafeRadiusTiles - d.commanderSafeRadiusTiles) >= 1e-4f) {
        diffs.push_back({"commanderSafeRadiusTiles", formatFloat(d.commanderSafeRadiusTiles), formatFloat(p.commanderSafeRadiusTiles)});
    }
    if (std::abs(p.artilleryStandOffMargin - d.artilleryStandOffMargin) >= 1e-4f) {
        diffs.push_back({"artilleryStandOffMargin", formatFloat(d.artilleryStandOffMargin), formatFloat(p.artilleryStandOffMargin)});
    }
    if (std::abs(p.artilleryIdealRangeRatio - d.artilleryIdealRangeRatio) >= 1e-4f) {
        diffs.push_back({"artilleryIdealRangeRatio", formatFloat(d.artilleryIdealRangeRatio), formatFloat(p.artilleryIdealRangeRatio)});
    }
    if (std::abs(p.scoutingWeight - d.scoutingWeight) >= 1e-4f) {
        diffs.push_back({"scoutingWeight", formatFloat(d.scoutingWeight), formatFloat(p.scoutingWeight)});
    }
    if (std::abs(p.focusFireWeight - d.focusFireWeight) >= 1e-4f) {
        diffs.push_back({"focusFireWeight", formatFloat(d.focusFireWeight), formatFloat(p.focusFireWeight)});
    }
    if (p.targetMemoryTicks != d.targetMemoryTicks) {
        diffs.push_back({"targetMemoryTicks", std::to_string(d.targetMemoryTicks), std::to_string(p.targetMemoryTicks)});
    }
    if (p.enableCommanderRetreat != d.enableCommanderRetreat) {
        diffs.push_back({"enableCommanderRetreat", d.enableCommanderRetreat ? "true" : "false", p.enableCommanderRetreat ? "true" : "false"});
    }
    if (p.enableUnitRetreat != d.enableUnitRetreat) {
        diffs.push_back({"enableUnitRetreat", d.enableUnitRetreat ? "true" : "false", p.enableUnitRetreat ? "true" : "false"});
    }
    if (p.scoutingGroupCount != d.scoutingGroupCount) {
        diffs.push_back({"scoutingGroupCount", std::to_string(d.scoutingGroupCount), std::to_string(p.scoutingGroupCount)});
    }
    if (std::abs(p.threatRadiusTiles - d.threatRadiusTiles) >= 1e-4f) {
        diffs.push_back({"threatRadiusTiles", formatFloat(d.threatRadiusTiles), formatFloat(p.threatRadiusTiles)});
    }
    return diffs;
}

/**
 * @brief Lưu bộ tham số AiParams kèm metadata huấn luyện ra file JSON chuẩn
 */
inline bool saveTrainedAiProfile(const std::string& filepath, const AiParams& params, const AiTrainingMetadata& meta) {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream f(filepath);
        if (!f.is_open()) return false;

        nlohmann::json root;
        root["metadata"] = meta;
        root["params"] = params;

        f << root.dump(4) << "\n";
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Nạp bộ tham số AiParams (và metadata nếu có) từ file JSON
 *
 * Hỗ trợ cả 2 định dạng:
 * - Dạng profile đầy đủ: {"metadata": {...}, "params": {...}}
 * - Dạng phẳng trực tiếp: {"aggression": 0.8, ...}
 */
inline bool loadTrainedAiProfile(const std::string& filepath, AiParams& outParams, AiTrainingMetadata* outMeta = nullptr) {
    try {
        std::ifstream f(filepath, std::ios::binary);
        if (!f.is_open()) return false;

        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.size() >= 2) {
            const auto b0 = static_cast<unsigned char>(content[0]);
            const auto b1 = static_cast<unsigned char>(content[1]);
            if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)) {
                std::cerr << "[AiTelemetry] LOI: File '" << filepath
                          << "' la UTF-16 (co BOM) - file là UTF-16, hãy lưu UTF-8!\n";
                return false;
            }
        }
        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content.erase(0, 3);
        }

        nlohmann::json root = nlohmann::json::parse(content);
        if (root.contains("params") && root["params"].is_object()) {
            from_json(root["params"], outParams);
            if (outMeta && root.contains("metadata") && root["metadata"].is_object()) {
                from_json(root["metadata"], *outMeta);
            }
        } else {
            from_json(root, outParams);
            if (outMeta && root.contains("metadata") && root["metadata"].is_object()) {
                from_json(root["metadata"], *outMeta);
            }
        }
        clampParams(outParams);
        return true;
    } catch (...) {
        return false;
    }
}

// =============================================================================
// Thống kê đánh giá (M9.11)
// =============================================================================
struct WilsonCI {
    double lower{0.0};
    double upper{0.0};
};

inline WilsonCI wilsonScoreInterval(int successes, int total) {
    if (total <= 0) return {0.0, 0.0};
    const double z = 1.96;
    const double n = static_cast<double>(total);
    const double p = static_cast<double>(successes) / n;
    const double z2 = z * z;
    const double denom = 1.0 + z2 / n;
    const double centre = p + z2 / (2.0 * n);
    const double spread = z * std::sqrt(std::max(0.0, (p * (1.0 - p) + z2 / (4.0 * n)) / n));
    return {
        std::max(0.0, (centre - spread) / denom),
        std::min(1.0, (centre + spread) / denom)
    };
}

inline double pairedSignTestPValue(int sweep2_0, int sweep0_2) {
    int n = sweep2_0 + sweep0_2;
    if (n <= 0) return 1.0;
    int k = std::min(sweep2_0, sweep0_2);
    double cdf = 0.0;
    double prob_i = std::pow(0.5, static_cast<double>(n));
    for (int i = 0; i <= k; ++i) {
        cdf += prob_i;
        if (i < n) {
            prob_i *= static_cast<double>(n - i) / static_cast<double>(i + 1);
        }
    }
    double pValue = std::min(1.0, 2.0 * cdf);
    return pValue;
}

/**
 * @brief Định dạng chuẩn 1 dòng Telemetry suy nghĩ của AI, căn thẳng cột trên CMD
 */
inline std::string formatTelemetryLine(const AiThoughtSnapshot& snap, int matchIndex, uint64_t currentTick, int tps) {
    char buf[512];
    const char* factionTag = (snap.faction == sim::Faction::Player) ? "AI1[BLUE]" : "AI2[RED ]";
    uint32_t totalSec = (tps > 0) ? static_cast<uint32_t>(currentTick / tps) : 0;
    uint32_t mm = totalSec / 60;
    uint32_t ss = totalSec % 60;

    int cmdHpPercent = (snap.commanderMaxHp > 0) ? (snap.commanderHp * 100 / snap.commanderMaxHp) : 0;

    // Chuẩn hóa Think: Posture và PrimaryIntent
    char thinkBuf[40];
    std::string intent = snap.primaryIntent.empty() ? "None" : snap.primaryIntent;
    if (intent.size() > 18) intent = intent.substr(0, 18);
    std::snprintf(thinkBuf, sizeof(thinkBuf), "THINK:%-10s(%s)", postureToString(snap.posture), intent.c_str());

    std::snprintf(buf, sizeof(buf),
        "%-9s M#%-2d T:%-5llu (%02u:%02u) | %-32s | P(agg:%.2f,ret:%.2f,cmdR:%4.1f,foc:%.2f,int:%2u) | Live:%2du %4dtr CmdHP:%3d%% | FoW:%2ds/%2dm | Cmds:+%-2u/%-4u | Roles:A%-2d/S%-2d/R%-2d",
        factionTag,
        matchIndex,
        static_cast<unsigned long long>(currentTick),
        mm, ss,
        thinkBuf,
        snap.params.aggression,
        snap.params.retreatHpRatio,
        snap.params.commanderSafeRadiusTiles,
        snap.params.focusFireWeight,
        snap.params.decisionIntervalTicks,
        snap.aliveUnits,
        snap.totalTroops,
        cmdHpPercent,
        snap.visibleEnemies,
        snap.rememberedEnemies,
        snap.commandsIssuedThisCycle,
        snap.totalCommandsIssued,
        snap.attackingUnits,
        snap.scoutingUnits,
        snap.retreatingUnits
    );
    return std::string(buf);
}

} // namespace ai
