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

/**
 * @brief Lớp UnitRenderer vẽ các chấm / khối quân đội lên màn hình.
 *
 * Mốc M3:
 * - Kích thước và tỉ lệ lấy từ JSON (tilePixels, iconSizeTiles).
 * - Vẽ màu theo phe: Player màu xanh dương, Enemy màu đỏ.
 * - Vẽ biểu tượng ngôi sao/tâm đặc biệt cho Tư lệnh.
 * - Vẽ thanh máu (Health bar) và hiển thị tương quan quân số.
 * - Frustum culling loại bỏ các đơn vị ngoài tầm nhìn camera.
 */
class UnitRenderer {
public:
    UnitRenderer() = default;
    ~UnitRenderer() = default;

    void render(SDL_Renderer* renderer,
                const std::vector<sim::Unit>& units,
                const Camera& camera,
                int screenWidth,
                int screenHeight,
                int tilePixels = 32);
};

} // namespace render
