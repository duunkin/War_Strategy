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
 * @brief Lớp UnitRenderer vẽ các sư đoàn quân đội lên màn hình.
 *
 * Mốc M3a:
 * - Vẽ mỗi sư đoàn là một hình vuông màu xanh có viền (chưa cần sprite).
 * - Frustum culling: loại bỏ các đơn vị ngoài màn hình.
 * - Hỗ trợ thu phóng camera mượt mà.
 */
class UnitRenderer {
public:
    UnitRenderer() = default;
    ~UnitRenderer() = default;

    void render(SDL_Renderer* renderer, const std::vector<sim::Unit>& units, const Camera& camera, int screenWidth, int screenHeight);
};

} // namespace render
