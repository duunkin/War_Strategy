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

    // 7. Tham số sinh Hồ nước ngọt nội địa (Lake)
    int lakeCount{2};                    // Số lượng hồ tự nhiên ở các thung lũng nội địa
    int lakeInlandMargin{10};            // Khoảng cách tối thiểu từ mép bản đồ để sinh hồ
    int lakeExpansionNeighbors{2};       // Số ô lân cận mở rộng xung quanh tâm hồ (tạo hình dáng tự nhiên)

    // 8. Tham số sinh Sông (River)
    int riverCount{4};                   // Số lượng dòng sông khởi nguồn từ núi/đồi
    int minRiverLength{5};               // Chiều dài tối thiểu để một con sông được công nhận
    int maxRiverLength{120};             // Chiều dài tối đa trước khi kết thúc
    int riverStartMargin{8};             // Tránh chọn nguồn sông quá sát mép bản đồ
};

} // namespace mapgen
