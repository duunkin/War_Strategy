#pragma once

#include <cstdint>
#include <vector>

namespace sim {

/**
 * @brief Lệnh di chuyển đơn vị tới một ô đích
 */
struct MoveCommand {
    std::vector<uint32_t> unitIds; // Danh sách ID các đơn vị được lệnh
    int targetTileX{0};           // Tọa độ X ô đích
    int targetTileY{0};           // Tọa độ Y ô đích
};

/**
 * @brief Loại lệnh
 */
enum class CommandType : uint8_t {
    Move = 1
};

/**
 * @brief Struct Command đại diện cho một lệnh gửi vào hàng đợi của sim
 */
struct Command {
    CommandType type{CommandType::Move};
    MoveCommand move;
};

} // namespace sim
