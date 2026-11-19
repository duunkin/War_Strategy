#pragma once

#include "MapGenConfig.hpp"
#include <cstdint>

namespace sim {
class WorldMap;
}

namespace mapgen {

/**
 * @brief Lớp MapGenerator phụ trách sinh bản đồ địa hình theo seed tất định.
 *
 * Quy trình sinh bản đồ:
 * 1. Tính độ cao từng ô bằng Simplex noise đa tầng (fBm). Giảm độ cao dần về phía mép để viền luôn là biển.
 * 2. Đảm bảo đúng tỉ lệ 5 đất : 1 biển: sắp xếp độ cao toàn bộ ô và lấy 1/6 số ô thấp nhất làm Biển (Sea).
 * 3. Phân loại địa hình đất liền dựa trên ngưỡng trong MapGenConfig: Núi cao (Mountain), Đồi (Hill), Đất bằng (Plain).
 * 4. Rừng (Forest): Sinh bằng Simplex noise thứ hai chỉ áp dụng trên các ô Đất bằng.
 * 5. Hồ (Lake): Tạo các vùng trũng hồ tự nhiên tại các thung lũng nội địa.
 * 6. Sông (River): Bắt đầu từ vùng núi/đồi cao, chảy dần xuống vùng trũng theo độ dốc tới khi gặp Biển hoặc Hồ.
 */
class MapGenerator {
public:
    MapGenerator() = default;
    ~MapGenerator() = default;

    // Sinh bản đồ vào đối tượng WorldMap theo seed và cấu hình cung cấp
    void generate(sim::WorldMap& map, uint64_t seed, const MapGenConfig& config = MapGenConfig{}) const;
};

} // namespace mapgen
