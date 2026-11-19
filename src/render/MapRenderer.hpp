#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

namespace sim {
class WorldMap;
}

namespace render {
class Camera;

/**
 * @brief Lớp MapRenderer chịu trách nhiệm vẽ bản đồ và hệ thống lưới ô vuông (grid).
 *
 * Tính năng:
 * - Vẽ nền đơn sắc cho toàn bộ bản đồ 128x96 ô.
 * - Áp dụng kỹ thuật culling (chỉ vẽ các đường lưới nằm trong khung nhìn camera).
 * - Vẽ đường biên rõ ràng bao quanh toàn bộ ranh giới bản đồ.
 */
class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer() = default;

    // Vẽ bản đồ và lưới ô lên renderer
    void render(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight);

private:
    // Màu nền bản đồ (xám đen chiến trường)
    SDL_Color m_backgroundColor{30, 34, 42, 255};

    // Màu đường kẻ lưới ô (xám nhạt hơn)
    SDL_Color m_gridColor{52, 58, 68, 255};

    // Màu đường viền bao quanh toàn bộ bản đồ thế giới
    SDL_Color m_borderColor{180, 150, 90, 255};
};

} // namespace render
