#include "UnitRenderer.hpp"
#include "Camera.hpp"
#include <algorithm>

namespace render {

void UnitRenderer::render(SDL_Renderer* renderer, const std::vector<sim::Unit>& units, const Camera& camera, int screenWidth, int screenHeight) {
    if (!renderer) return;

    float zoom = camera.getZoom();
    float baseSize = 22.0f * zoom;
    if (baseSize < 6.0f) baseSize = 6.0f;

    for (const auto& unit : units) {
        // Chuyển đổi mili-ô sang pixel thế giới: 1 ô = 1000 mili-ô = 32 pixel
        float worldX = static_cast<float>(unit.getPosX()) * 32.0f / 1000.0f;
        float worldY = static_cast<float>(unit.getPosY()) * 32.0f / 1000.0f;

        // Chuyển sang tọa độ màn hình
        float screenX = camera.worldToScreenX(worldX);
        float screenY = camera.worldToScreenY(worldY);

        // Frustum culling
        if (screenX + baseSize < 0.0f || screenX - baseSize > static_cast<float>(screenWidth) ||
            screenY + baseSize < 0.0f || screenY - baseSize > static_cast<float>(screenHeight)) {
            continue;
        }

        int rx = static_cast<int>(screenX - baseSize * 0.5f);
        int ry = static_cast<int>(screenY - baseSize * 0.5f);
        int rsize = static_cast<int>(baseSize);

        SDL_Rect rect{rx, ry, rsize, rsize};

        // 1. Thân hình vuông màu xanh dương của sư đoàn phe người chơi
        SDL_SetRenderDrawColor(renderer, 30, 110, 230, 255);
        SDL_RenderFillRect(renderer, &rect);

        // 2. Viền ngoài
        if (unit.isSelected()) {
            // Viền vàng sáng nếu đang được chọn
            SDL_SetRenderDrawColor(renderer, 255, 230, 70, 255);
            SDL_RenderDrawRect(renderer, &rect);
            SDL_Rect outerRect{rx - 1, ry - 1, rsize + 2, rsize + 2};
            SDL_RenderDrawRect(renderer, &outerRect);
        } else {
            // Viền trắng sáng tiêu chuẩn
            SDL_SetRenderDrawColor(renderer, 240, 245, 255, 255);
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

} // namespace render
