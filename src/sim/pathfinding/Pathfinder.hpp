#pragma once

#include "../map/WorldMap.hpp"
#include "../config/SimConfig.hpp"
#include <vector>
#include <cstdint>

namespace sim {

/**
 * @brief Tọa độ ô vuông nguyên trên bản đồ
 */
struct TileCoord {
    int x{0};
    int y{0};

    bool operator==(const TileCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

/**
 * @brief Kết quả tìm đường A*
 */
struct PathResult {
    bool found{false};              // Có tìm thấy đường đến đích (hoặc đích điều chỉnh)
    bool adjusted{false};           // Đích ban đầu không đến được, đã điều chỉnh về ô gần nhất
    TileCoord originalTarget{0, 0}; // Đích gốc người chơi chỉ định
    TileCoord finalTarget{0, 0};    // Đích thực tế (sau khi điều chỉnh nếu cần)
    std::vector<TileCoord> waypoints; // Danh sách các waypoint từ bước tiếp theo đến finalTarget
};

/**
 * @brief Thuật toán tìm đường A* trên lưới bản đồ 128x96
 *
 * Quy tắc chuẩn M4:
 * - Chi phí mỗi ô = 1 / hệ số tốc độ (tính bằng số nguyên với độ chính xác cao).
 * - Đi chéo chi phí x1.414, cấm cắt góc qua hai ô chéo bị chặn.
 * - Bộ binh, xe, pháo, tư lệnh: đi trên đất liền theo hệ số miền.
 * - Tàu: chỉ đi trên biển/hồ (DEEP_SEA, COASTAL_SEA).
 * - Máy bay: bay thẳng, bỏ qua địa hình (chỉ 1 waypoint tới đích).
 * - Đích không đến được: tự động đưa về ô gần nhất mà đơn vị đến được.
 * - Thuần số nguyên, tất định 100%, không dùng float trong logic sim.
 */
class Pathfinder {
public:
    static PathResult findPath(const WorldMap& map,
                               const SimConfig& config,
                               UnitDomain domain,
                               UnitType unitType,
                               TileCoord start,
                               TileCoord target);

    // Tiện ích rải các ô đích quanh tâm target cho nhóm N đơn vị để không chồng lên nhau
    static std::vector<TileCoord> disperseDestinations(const WorldMap& map,
                                                       const SimConfig& config,
                                                       UnitDomain domain,
                                                       UnitType unitType,
                                                       TileCoord centerTarget,
                                                       size_t unitCount);
};

} // namespace sim
