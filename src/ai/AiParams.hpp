#pragma once

#include <cstdint>

namespace ai {

/**
 * @brief Tham số chiến thuật cho AI điều khiển phe phái (M7)
 *
 * Sẵn sàng cho việc tối ưu tự động (M9) và chạy AI vs AI (M8).
 * Toàn bộ các giá trị đều có mặc định chuẩn hóa theo nhịp mô phỏng 20 tick/s.
 */
struct AiParams {
    // Nhịp độ ra quyết định (ticks). Mặc định 20 ticks = 1 giây mỗi lần tính toán chiến thuật lớn.
    uint32_t decisionIntervalTicks{20};

    // Độ hung hăng (0.0: phòng thủ bị động -> 1.0: tổng lực tấn công truy sát)
    float aggression{0.75f};

    // Tỉ lệ máu để kích hoạt rút lui chiến thuật bảo toàn lực lượng (0.0 -> 1.0)
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
};

} // namespace ai
