#pragma once

#include <cstdint>

namespace mapgen {

/**
 * @brief Cấu hình các tham số và ngưỡng sinh bản đồ thế giới (MapGenConfig).
 *
 * Tất cả các ngưỡng và tham số sinh địa hình được gom vào struct này ở một chỗ duy nhất
 * để tiện tùy chỉnh, cân bằng mà không cần can thiệp vào thuật toán lõi.
 */
struct MapGenConfig {
    // 1. Kích thước bản đồ chuẩn
    int width{128};                      // Chiều rộng bản đồ (số ô ngang, mặc định 128)
    int height{96};                      // Chiều cao bản đồ (số ô dọc, mặc định 96)

    // 2. Tỉ lệ đất / biển: 5 đất : 1 biển (tổng 6 phần, biển chiếm 1/6 số ô thấp nhất)
    int seaRatioNumerator{1};            // Tử số tỉ lệ biển (1)
    int seaRatioDenominator{6};          // Mẫu số tỉ lệ biển (6)

    // 3. Vùng mép giảm độ cao (Border falloff)
    // Khoảng cách từ mép bản đồ (tính bằng số ô) bắt đầu giảm dần độ cao để đảm bảo viền luôn là biển
    int borderFalloffDistance{14};
    int borderFalloffStrength{300};      // Cường độ kéo tụt độ cao ở sát mép

    // 4. Ngưỡng phân chia địa hình đất liền (tính theo phần nghìn trên tổng số ô đất)
    // Tổng số ô đất = (128x96) * 5/6 = 10240 ô
    int mountainPermille{120};           // 120/1000 = 12% số ô đất cao nhất là Núi (~1228 ô)
    int hillPermille{250};               // 250/1000 = 25% số ô đất tiếp theo là Đồi (~2560 ô)
    // Còn lại (1000 - 120 - 250 = 630/1000 = 63%) là Đất bằng (Plain)

    // 5. Tham số Simplex noise độ cao (Elevation Noise)
    int elevationOctaves{4};             // Số tầng octave
    int elevationFrequencyFP{800};       // Tần số cơ bản Q16.16 (giá trị càng nhỏ mảng lục địa càng to)
    int elevationPersistenceFP{32768};   // Hệ số suy giảm biên độ (32768 = 0.5 trong Q16.16)

    // 6. Tham số sinh Rừng (Forest Noise - noise thứ 2 trên đất bằng)
    int forestOctaves{3};                // Số tầng octave cho rừng
    int forestFrequencyFP{1600};         // Tần số cho mảng rừng
    int forestThresholdFP{2000};         // Ngưỡng mọc rừng: noise > threshold sẽ mọc rừng (~35-40% đất bằng)

    // 7. Tham số sinh Hồ nước ngọt nội địa (Lake) - M2 cập nhật
    int lakeMax{6};                      // Số lượng hồ tối đa (ngẫu nhiên 0 đến lakeMax, mặc định 6)
    int lakeMinRadius{2};                // Bán kính hồ tối thiểu (vài ô)
    int lakeMaxRadius{3};                // Bán kính hồ tối đa (vài ô)

    // 8. Tham số sinh Sông và Bến cạn (River & Ford) - M2 cập nhật
    int riverFordCount{3};               // Số lượng chỗ cạn (ford, mặc định 3, cho phép 0)
    int riverFordMinDistance{15};         // Khoảng cách tối thiểu giữa các ford và từ ford tới đầu mút sông (>= 15 ô)
    float riverMaxDeviation{3.0f};       // Độ lệch tối đa uốn lượn của sông (vài ô)
    float minLandRatio{0.40f};           // Tỉ lệ diện tích đất tối thiểu mỗi nửa map (40%)
    int riverMaxRetries{100};            // Số lần thử tối đa khi sinh trục sông

    // 9. Căn cứ và vùng xuất phát 2 phe
    int playerBaseX{20};
    int playerBaseY{48};
    int enemyBaseX{107};
    int enemyBaseY{48};
    int baseRadius{8};

    // 10. Điểm mút sông cố định cho trường hợp đặc biệt (-1 nghĩa là sinh ngẫu nhiên theo seed)
    int fixedP1X{-1};
    int fixedP1Y{-1};
    int fixedP2X{-1};
    int fixedP2Y{-1};
};

} // namespace mapgen
