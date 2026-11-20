#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "../sim/units/Unit.hpp"
#include <vector>

namespace render {
class Camera;
class TextRenderer;

/**
 * @brief Lớp UnitRenderer vẽ các chấm quân đội, vòng chọn sáng, đường đi A* và bảng thông tin.
 *
 * Mốc M4:
 * - Vẽ vòng sáng cho các chấm đang được chọn.
 * - Vẽ đường đi nối từ vị trí hiện tại qua các waypoints tới đích (màu sáng thanh mảnh, tự ngắn lại).
 * - Vẽ dấu đích đến: dấu bình thường hoặc dấu cảnh báo nếu đích đã được điều chỉnh.
 * - Vẽ khung chữ nhật kéo thả chọn quân (box selection).
 * - Vẽ bảng thông tin nhỏ hiển thị chi tiết đơn vị (loại, quân số, máu, tốc độ).
 */
class UnitRenderer {
public:
    UnitRenderer() = default;
    ~UnitRenderer() = default;

    // Vẽ toàn bộ đơn vị và các đường đi của các đơn vị đang chọn
    void render(SDL_Renderer* renderer,
                const std::vector<sim::Unit>& units,
                const Camera& camera,
                int screenWidth,
                int screenHeight,
                int tilePixels = 32);

    // Vẽ khung chữ nhật kéo thả chọn quân
    void renderSelectionBox(SDL_Renderer* renderer, int startX, int startY, int currentX, int currentY);

    // Vẽ bảng thông tin nhỏ cho đơn vị đang chọn / đang xem
    void renderInfoPanel(SDL_Renderer* renderer,
                         const sim::Unit& unit,
                         TextRenderer& textRenderer,
                         int screenWidth,
                         int screenHeight);
};

} // namespace render
