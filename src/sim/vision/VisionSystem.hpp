#pragma once

// Quy tắc chắn tầm nhìn: Tia nhìn từ ô quan sát đến ô mục tiêu bị chắn nếu tồn tại bất kỳ ô trung gian nào có độ cao vượt quá đường dốc độ cao nối giữa hai ô, hoặc ô trung gian đó có cùng độ cao với tia nhìn nằm ngang trên địa hình cản trở (rừng, đồi, núi).

/*
 * BẢNG VÍ DỤ TẦM NHÌN THEO ĐỘ CAO VÀ ĐỊA HÌNH:
 * +--------------------+-------------------+-----------------------+-------------------------------------------------------+
 * | Điểm quan sát      | Mục tiêu          | Địa hình chắn ở giữa  | Kết quả tầm nhìn                                      |
 * +--------------------+-------------------+-----------------------+-------------------------------------------------------+
 * | Rừng (h=1)         | Đồng bằng (h=0)   | Không có              | Thấy ô đồng bằng sát mép; không nhìn xuyên qua rừng   |
 * | Đồng bằng (h=0)    | Rừng (h=1)        | Không có              | Thấy ô rừng ở rìa; không nhìn xuyên sâu qua rừng      |
 * | Đồi (h=2)          | Đồi khác (h=2)    | Đồng bằng/Rừng (h<=1) | Thấy mục tiêu vượt qua vùng trũng ở giữa              |
 * | Đồi (h=2)          | Đồi khác (h=2)    | Núi (h=3) / Đồi (h=2) | Bị chắn hoàn toàn bởi núi hoặc đồi cùng độ cao        |
 * | Núi (h=3)          | Đồng bằng (h=0)   | Đồi (h=2) / Rừng (h=1)| Thấy mục tiêu nếu vật cản không vượt quá đường dốc    |
 * | Máy bay (h=4)      | Mọi địa hình      | Mọi địa hình          | Thấy mọi mục tiêu trong bán kính; không bị che        |
 * | Bất kỳ vị trí nào  | Máy bay địch (h=4)| Mọi địa hình          | Máy bay địch trong bán kính luôn bị lộ                |
 * +--------------------+-------------------+-----------------------+-------------------------------------------------------+
 */

#include "../units/Unit.hpp"
#include "../map/WorldMap.hpp"
#include "../config/SimConfig.hpp"

#include <vector>
#include <cstdint>
#include <functional>

namespace sim {

/**
 * @brief Ba trạng thái của một ô trong hệ thống Sương mù chiến tranh (Fog of War)
 */
enum class FogState : uint8_t {
    Unexplored = 0, // Chưa từng khám phá (màu đen tuyền)
    Explored = 1,   // Đã từng khám phá nhưng hiện không thấy (màu tối mờ, chỉ hiện địa hình)
    Visible = 2     // Đang nhìn thấy trực tiếp (đầy đủ màu sắc và phát hiện quân địch)
};

class VisionSystem {
public:
    /**
     * @brief Kiểm tra đường thẳng tầm nhìn (Line of Sight) với mảng độ cao phẳng tối ưu hiệu năng cao
     */
    static bool isLineOfSightClear(int x0, int y0, int h0,
                                  int x1, int y1, int h1,
                                  const uint8_t* elevationGrid,
                                  int mapWidth,
                                  int airElevation = 4);

    /**
     * @brief Kiểm tra đường thẳng tầm nhìn (Line of Sight) với hàm callback độ cao linh hoạt
     */
    static bool isLineOfSightClear(int x0, int y0, int h0,
                                  int x1, int y1, int h1,
                                  const std::function<int(int, int)>& getElevationFunc,
                                  int airElevation = 4);

    /**
     * @brief Cập nhật bản đồ sương mù cho toàn bộ một phe phái (dùng chung cho cả phe)
     */
    static void updateFactionVision(Faction faction,
                                    const std::vector<Unit>& units,
                                    const WorldMap& map,
                                    const SimConfig& config,
                                    const uint8_t* elevationGrid,
                                    std::vector<uint8_t>& fogMap);

    static void updateFactionVision(Faction faction,
                                    const std::vector<Unit>& units,
                                    const WorldMap& map,
                                    const SimConfig& config,
                                    std::vector<uint8_t>& fogMap);

    /**
     * @brief Kiểm tra xem một đơn vị địch có đang hiển thị với phe quan sát hay không
     */
    static bool isUnitVisibleToFaction(const Unit& targetUnit,
                                       Faction observerFaction,
                                       const std::vector<Unit>& allUnits,
                                       const std::vector<uint8_t>& observerFogMap,
                                       int mapWidth, int mapHeight);
};

} // namespace sim
