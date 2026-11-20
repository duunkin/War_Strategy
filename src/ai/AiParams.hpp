#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace ai {

/**
 * @brief Tham số chiến thuật cho AI điều khiển phe phái (M7/M9)
 *
 * Chuẩn hóa cho hệ thống tự tiến hóa (Evolutionary Self-Play) và thích nghi (Post-Loss Adaptation).
 * Toàn bộ các giá trị đều có mặc định chuẩn hóa theo nhịp mô phỏng 20 tick/s.
 */
struct AiParams {
    // Nhịp độ ra quyết định (ticks). Mặc định 20 ticks = 1 giây mỗi lần tính toán chiến thuật lớn.
    uint32_t decisionIntervalTicks{20};

    // Độ hung hăng (0.15: phòng thủ bị động -> 1.50: tổng lực tấn công truy sát)
    float aggression{0.75f};

    // Tỉ lệ máu để kích hoạt rút lui chiến thuật bảo toàn lực lượng (0.05 -> 0.70)
    float retreatHpRatio{0.30f};

    // Bán kính an toàn của Tư lệnh (số ô). Nếu kẻ địch lọt vào bán kính này, Tư lệnh sẽ tháo chạy
    float commanderSafeRadiusTiles{10.0f};

    // Khoảng cách an toàn tối thiểu giữa Caesar và mục tiêu địch (tránh bị áp sát)
    float artilleryStandOffMargin{2.0f};

    // Khoảng cách lý tưởng pháo Caesar duy trì với mục tiêu (tính theo tỉ lệ rangeTiles)
    float artilleryIdealRangeRatio{0.80f};

    // Trọng số trinh sát (thúc đẩy các đơn vị cơ động mở sương mù chiến tranh)
    float scoutingWeight{1.0f};

    // Trọng số tập trung hỏa lực (ưu tiên dồn sát thương vào mục tiêu trọng yếu đã phát hiện)
    float focusFireWeight{1.2f};

    // Thời gian lưu vị trí cuối cùng nhìn thấy địch trong bộ nhớ (ticks, 140 ticks = 7 giây)
    uint32_t targetMemoryTicks{140};

    // Bật/tắt cơ chế rút lui né giao tranh cho Tư lệnh
    bool enableCommanderRetreat{true};

    // Bật/tắt cơ chế rút lui bảo toàn quân số khi máu thấp cho các đơn vị thường
    bool enableUnitRetreat{true};

    // Số mũi trinh sát độc lập khi chưa phát hiện địch
    int scoutingGroupCount{3};

    bool operator==(const AiParams& o) const {
        return decisionIntervalTicks == o.decisionIntervalTicks &&
               std::abs(aggression - o.aggression) < 1e-5f &&
               std::abs(retreatHpRatio - o.retreatHpRatio) < 1e-5f &&
               std::abs(commanderSafeRadiusTiles - o.commanderSafeRadiusTiles) < 1e-5f &&
               std::abs(artilleryStandOffMargin - o.artilleryStandOffMargin) < 1e-5f &&
               std::abs(artilleryIdealRangeRatio - o.artilleryIdealRangeRatio) < 1e-5f &&
               std::abs(scoutingWeight - o.scoutingWeight) < 1e-5f &&
               std::abs(focusFireWeight - o.focusFireWeight) < 1e-5f &&
               targetMemoryTicks == o.targetMemoryTicks &&
               enableCommanderRetreat == o.enableCommanderRetreat &&
               enableUnitRetreat == o.enableUnitRetreat &&
               scoutingGroupCount == o.scoutingGroupCount;
    }

    bool operator!=(const AiParams& o) const {
        return !(*this == o);
    }
};

/**
 * @brief Giới hạn chặt mọi tham số trong khoảng hợp lệ về mặt chiến thuật và vật lý (M9)
 *
 * Đảm bảo:
 * - Tránh số âm, NaN, Inf
 * - decisionIntervalTicks trong [8, 40] (tránh lag A* nếu quá nhỏ hoặc đơ nếu quá lớn)
 * - aggression trong [0.15, 1.50]
 * - retreatHpRatio trong [0.05, 0.70]
 * - commanderSafeRadiusTiles trong [4.0, 22.0]
 * - artilleryStandOffMargin trong [1.0, 6.0]
 * - artilleryIdealRangeRatio trong [0.50, 0.95]
 * - scoutingWeight trong [0.10, 3.00]
 * - focusFireWeight trong [0.20, 3.00]
 * - targetMemoryTicks trong [20, 400] (1.0s đến 20.0s)
 * - scoutingGroupCount trong [1, 6]
 */
inline void clampParams(AiParams& p) {
    auto sanitizeFloat = [](float& val, float fallback, float minVal, float maxVal) {
        if (std::isnan(val) || std::isinf(val)) {
            val = fallback;
        }
        val = std::clamp(val, minVal, maxVal);
    };

    p.decisionIntervalTicks = std::clamp(p.decisionIntervalTicks, 8u, 40u);
    sanitizeFloat(p.aggression, 0.75f, 0.15f, 1.50f);
    sanitizeFloat(p.retreatHpRatio, 0.30f, 0.05f, 0.70f);
    sanitizeFloat(p.commanderSafeRadiusTiles, 10.0f, 4.0f, 22.0f);
    sanitizeFloat(p.artilleryStandOffMargin, 2.0f, 1.0f, 6.0f);
    sanitizeFloat(p.artilleryIdealRangeRatio, 0.80f, 0.50f, 0.95f);
    sanitizeFloat(p.scoutingWeight, 1.0f, 0.10f, 3.00f);
    sanitizeFloat(p.focusFireWeight, 1.2f, 0.20f, 3.00f);
    p.targetMemoryTicks = std::clamp(p.targetMemoryTicks, 20u, 400u);
    p.scoutingGroupCount = std::clamp(p.scoutingGroupCount, 1, 6);
}

} // namespace ai
