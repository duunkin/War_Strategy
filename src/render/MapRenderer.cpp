#include "MapRenderer.hpp"
#include "Camera.hpp"
#include "sim/map/WorldMap.hpp"

#include <algorithm>
#include <cmath>

namespace render {

MapRenderer::MapRenderer() = default;

void MapRenderer::render(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight) {
    if (!renderer) return;

    float zoom = camera.getZoom();
    int tileSize = map.getTileSize();
    int worldWidth = map.getWorldWidth();
    int worldHeight = map.getWorldHeight();

    // 1. Tính toán vùng màn hình của bản đồ
    float mapScreenLeft = camera.worldToScreenX(0.0f);
    float mapScreenTop = camera.worldToScreenY(0.0f);
    float mapScreenW = static_cast<float>(worldWidth) * zoom;
    float mapScreenH = static_cast<float>(worldHeight) * zoom;

    // Giới hạn vùng vẽ thực tế trong phạm vi hiển thị
    SDL_Rect mapRect{
        .x = static_cast<int>(std::round(mapScreenLeft)),
        .y = static_cast<int>(std::round(mapScreenTop)),
        .w = static_cast<int>(std::round(mapScreenW)),
        .h = static_cast<int>(std::round(mapScreenH))
    };

    // Vẽ nền bản đồ đơn sắc
    SDL_SetRenderDrawColor(renderer, m_backgroundColor.r, m_backgroundColor.g, m_backgroundColor.b, m_backgroundColor.a);
    SDL_RenderFillRect(renderer, &mapRect);

    // 2. Tính toán chỉ số các hàng và cột nằm trong tầm nhìn (Frustum Culling)
    // Tránh việc duyệt qua toàn bộ 128x96 ô gây lãng phí hiệu năng
    float viewLeftWorld = std::max(0.0f, camera.getX());
    float viewRightWorld = std::min(static_cast<float>(worldWidth), camera.getX() + (static_cast<float>(screenWidth) / zoom));
    float viewTopWorld = std::max(0.0f, camera.getY());
    float viewBottomWorld = std::min(static_cast<float>(worldHeight), camera.getY() + (static_cast<float>(screenHeight) / zoom));

    int minCol = std::max(0, static_cast<int>(viewLeftWorld / tileSize));
    int maxCol = std::min(map.getWidth(), static_cast<int>(std::ceil(viewRightWorld / tileSize)));

    int minRow = std::max(0, static_cast<int>(viewTopWorld / tileSize));
    int maxRow = std::min(map.getHeight(), static_cast<int>(std::ceil(viewBottomWorld / tileSize)));

    // 3. Vẽ các đường kẻ lưới ô đơn sắc
    SDL_SetRenderDrawColor(renderer, m_gridColor.r, m_gridColor.g, m_gridColor.b, m_gridColor.a);

    // Đoạn giới hạn Y cho các đường dọc
    int lineY1 = std::max(0, static_cast<int>(std::round(mapScreenTop)));
    int lineY2 = std::min(screenHeight, static_cast<int>(std::round(mapScreenTop + mapScreenH)));

    // Vẽ các đường dọc
    for (int col = minCol; col <= maxCol; ++col) {
        float worldX = static_cast<float>(col * tileSize);
        int screenX = static_cast<int>(std::round(camera.worldToScreenX(worldX)));
        if (screenX >= 0 && screenX <= screenWidth) {
            SDL_RenderDrawLine(renderer, screenX, lineY1, screenX, lineY2);
        }
    }

    // Đoạn giới hạn X cho các đường ngang
    int lineX1 = std::max(0, static_cast<int>(std::round(mapScreenLeft)));
    int lineX2 = std::min(screenWidth, static_cast<int>(std::round(mapScreenLeft + mapScreenW)));

    // Vẽ các đường ngang
    for (int row = minRow; row <= maxRow; ++row) {
        float worldY = static_cast<float>(row * tileSize);
        int screenY = static_cast<int>(std::round(camera.worldToScreenY(worldY)));
        if (screenY >= 0 && screenY <= screenHeight) {
            SDL_RenderDrawLine(renderer, lineX1, screenY, lineX2, screenY);
        }
    }

    // 4. Vẽ đường viền bao quanh toàn bộ ranh giới bản đồ (Border)
    SDL_SetRenderDrawColor(renderer, m_borderColor.r, m_borderColor.g, m_borderColor.b, m_borderColor.a);
    SDL_RenderDrawRect(renderer, &mapRect);
}

} // namespace render
