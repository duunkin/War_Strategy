#pragma once

#include "AiParams.hpp"
#include "../sim/units/Unit.hpp"

#include <string>
#include <cstdint>
#include <vector>
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
        {"scoutingGroupCount", p.scoutingGroupCount}
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
