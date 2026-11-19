#include "MapRenderer.hpp"
#include "Camera.hpp"
#include "sim/map/WorldMap.hpp"

#include <algorithm>
#include <cmath>

namespace render {

MapRenderer::MapRenderer() = default;

MapRenderer::~MapRenderer() {
    cleanup();
}

void MapRenderer::cleanup() {
    if (m_minimapTexture) {
        SDL_DestroyTexture(m_minimapTexture);
        m_minimapTexture = nullptr;
    }
    m_lastMapHash = 0;
}

SDL_Color MapRenderer::getTerrainColor(sim::TerrainType type) {
    switch (type) {
        case sim::TerrainType::Sea:
            return SDL_Color{34, 86, 160, 255};   // Biển: Xanh đại dương
        case sim::TerrainType::Plain:
            return SDL_Color{88, 148, 64, 255};   // Đất bằng: Xanh cỏ tươi
        case sim::TerrainType::Hill:
            return SDL_Color{152, 138, 76, 255};  // Đồi: Nâu vàng đất đồi
        case sim::TerrainType::Mountain:
            return SDL_Color{124, 126, 132, 255}; // Núi: Xám đá hùng vĩ
        case sim::TerrainType::Forest:
            return SDL_Color{38, 92, 42, 255};    // Rừng: Xanh thẫm lá rậm
        case sim::TerrainType::River:
            return SDL_Color{68, 168, 226, 255};  // Sông: Xanh lam sáng lấp lánh
        case sim::TerrainType::Lake:
            return SDL_Color{48, 118, 192, 255};  // Hồ: Xanh dương nước ngọt tĩnh lặng
        default:
            return SDL_Color{50, 50, 50, 255};
    }
}

void MapRenderer::updateMinimapTexture(SDL_Renderer* renderer, const sim::WorldMap& map) {
    if (!renderer) return;

    int mapW = map.getWidth();
    int mapH = map.getHeight();

    // Tạo texture streaming 128x96 nếu chưa có
    if (!m_minimapTexture) {
        m_minimapTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            mapW,
            mapH
        );
        if (!m_minimapTexture) return;
        SDL_SetTextureScaleMode(m_minimapTexture, SDL_ScaleModeNearest);
    }

    void* pixelsPtr = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_minimapTexture, nullptr, &pixelsPtr, &pitch) == 0) {
        auto* pixelData = static_cast<uint32_t*>(pixelsPtr);
        int pitchElements = pitch / 4;

        for (int y = 0; y < mapH; ++y) {
            for (int x = 0; x < mapW; ++x) {
                sim::TerrainType t = map.getTile(x, y);
                SDL_Color c = getTerrainColor(t);
                // Định dạng RGBA8888: R (24..31), G (16..23), B (8..15), A (0..7)
                uint32_t colorValue = (static_cast<uint32_t>(c.r) << 24) |
                                      (static_cast<uint32_t>(c.g) << 16) |
                                      (static_cast<uint32_t>(c.b) << 8)  |
                                      (static_cast<uint32_t>(c.a));
                pixelData[y * pitchElements + x] = colorValue;
            }
        }
        SDL_UnlockTexture(m_minimapTexture);
    }

    m_lastMapHash = map.computeHash();
}

SDL_Rect MapRenderer::getMinimapRect(int screenWidth, int screenHeight) const {
    return SDL_Rect{
        .x = screenWidth - m_minimapWidth - m_minimapMargin,
        .y = screenHeight - m_minimapHeight - m_minimapMargin,
        .w = m_minimapWidth,
        .h = m_minimapHeight
    };
}

bool MapRenderer::handleMinimapClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, Camera& camera) const {
    SDL_Rect miniRect = getMinimapRect(screenWidth, screenHeight);

    if (mouseX < miniRect.x || mouseX >= miniRect.x + miniRect.w ||
        mouseY < miniRect.y || mouseY >= miniRect.y + miniRect.h) {
        return false;
    }

    // Tỉ lệ tọa độ tương đối [0..1] trên minimap
    float u = static_cast<float>(mouseX - miniRect.x) / static_cast<float>(miniRect.w);
    float v = static_cast<float>(mouseY - miniRect.y) / static_cast<float>(miniRect.h);
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float targetWorldX = u * static_cast<float>(map.getWorldWidth());
    float targetWorldY = v * static_cast<float>(map.getWorldHeight());

    // Căn camera sao cho tâm màn hình rơi đúng vào vị trí click
    float viewW = static_cast<float>(screenWidth) / camera.getZoom();
    float viewH = static_cast<float>(screenHeight) / camera.getZoom();

    camera.setPosition(targetWorldX - (viewW * 0.5f), targetWorldY - (viewH * 0.5f));
    camera.clampToBounds(screenWidth, screenHeight, map.getWorldWidth(), map.getWorldHeight());

    return true;
}

void MapRenderer::render(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight) {
    if (!renderer) return;

    // Tự động nạp minimap texture nếu bản đồ thay đổi hoặc texture chưa được tạo
    uint64_t currentMapHash = map.computeHash();
    if (!m_minimapTexture || m_lastMapHash != currentMapHash) {
        updateMinimapTexture(renderer, map);
    }

    float zoom = camera.getZoom();
    int tileSize = map.getTileSize();
    int worldWidth = map.getWorldWidth();
    int worldHeight = map.getWorldHeight();

    // 1. Tính toán vùng nhìn thế giới & Frustum culling
    float viewLeftWorld   = std::max(0.0f, camera.getX());
    float viewRightWorld  = std::min(static_cast<float>(worldWidth), camera.getX() + (static_cast<float>(screenWidth) / zoom));
    float viewTopWorld    = std::max(0.0f, camera.getY());
    float viewBottomWorld = std::min(static_cast<float>(worldHeight), camera.getY() + (static_cast<float>(screenHeight) / zoom));

    int minCol = std::max(0, static_cast<int>(viewLeftWorld / tileSize));
    int maxCol = std::min(map.getWidth() - 1, static_cast<int>(std::floor(viewRightWorld / tileSize)));

    int minRow = std::max(0, static_cast<int>(viewTopWorld / tileSize));
    int maxRow = std::min(map.getHeight() - 1, static_cast<int>(std::floor(viewBottomWorld / tileSize)));

    // 2. Vẽ từng ô bản đồ theo loại địa hình
    for (int row = minRow; row <= maxRow; ++row) {
        float worldY1 = static_cast<float>(row * tileSize);
        float worldY2 = static_cast<float>((row + 1) * tileSize);
        int screenY1 = static_cast<int>(std::round(camera.worldToScreenY(worldY1)));
        int screenY2 = static_cast<int>(std::round(camera.worldToScreenY(worldY2)));
        int tileH = screenY2 - screenY1;

        for (int col = minCol; col <= maxCol; ++col) {
            float worldX1 = static_cast<float>(col * tileSize);
            float worldX2 = static_cast<float>((col + 1) * tileSize);
            int screenX1 = static_cast<int>(std::round(camera.worldToScreenX(worldX1)));
            int screenX2 = static_cast<int>(std::round(camera.worldToScreenX(worldX2)));
            int tileW = screenX2 - screenX1;

            SDL_Rect tileRect{screenX1, screenY1, tileW, tileH};

            sim::TerrainType t = map.getTile(col, row);
            SDL_Color c = getTerrainColor(t);

            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }

    // 3. Vẽ đường lưới ô tinh tế (khi mức zoom đủ rõ)
    if (zoom >= 0.45f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, m_gridColor.r, m_gridColor.g, m_gridColor.b, m_gridColor.a);

        float mapScreenLeft = camera.worldToScreenX(0.0f);
        float mapScreenTop  = camera.worldToScreenY(0.0f);
        float mapScreenW    = static_cast<float>(worldWidth) * zoom;
        float mapScreenH    = static_cast<float>(worldHeight) * zoom;

        int lineY1 = std::max(0, static_cast<int>(std::round(mapScreenTop)));
        int lineY2 = std::min(screenHeight, static_cast<int>(std::round(mapScreenTop + mapScreenH)));

        for (int col = minCol; col <= maxCol + 1; ++col) {
            float worldX = static_cast<float>(col * tileSize);
            int screenX = static_cast<int>(std::round(camera.worldToScreenX(worldX)));
            if (screenX >= 0 && screenX <= screenWidth) {
                SDL_RenderDrawLine(renderer, screenX, lineY1, screenX, lineY2);
            }
        }

        int lineX1 = std::max(0, static_cast<int>(std::round(mapScreenLeft)));
        int lineX2 = std::min(screenWidth, static_cast<int>(std::round(mapScreenLeft + mapScreenW)));

        for (int row = minRow; row <= maxRow + 1; ++row) {
            float worldY = static_cast<float>(row * tileSize);
            int screenY = static_cast<int>(std::round(camera.worldToScreenY(worldY)));
            if (screenY >= 0 && screenY <= screenHeight) {
                SDL_RenderDrawLine(renderer, lineX1, screenY, lineX2, screenY);
            }
        }
    }

    // 4. Đường viền bao quanh toàn bộ ranh giới thế giới
    float mapScreenLeft = camera.worldToScreenX(0.0f);
    float mapScreenTop  = camera.worldToScreenY(0.0f);
    float mapScreenW    = static_cast<float>(worldWidth) * zoom;
    float mapScreenH    = static_cast<float>(worldHeight) * zoom;

    SDL_Rect mapBoundsRect{
        .x = static_cast<int>(std::round(mapScreenLeft)),
        .y = static_cast<int>(std::round(mapScreenTop)),
        .w = static_cast<int>(std::round(mapScreenW)),
        .h = static_cast<int>(std::round(mapScreenH))
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, m_borderColor.r, m_borderColor.g, m_borderColor.b, m_borderColor.a);
    SDL_RenderDrawRect(renderer, &mapBoundsRect);

    // =========================================================================
    // 5. Hiển thị MINIMAP ở góc dưới phải màn hình
    // =========================================================================
    SDL_Rect miniDstRect = getMinimapRect(screenWidth, screenHeight);

    // a) Khung viền và nền đen bao quanh Minimap
    SDL_Rect miniFrameRect{
        .x = miniDstRect.x - 3,
        .y = miniDstRect.y - 3,
        .w = miniDstRect.w + 6,
        .h = miniDstRect.h + 6
    };
    SDL_SetRenderDrawColor(renderer, 18, 20, 26, 230);
    SDL_RenderFillRect(renderer, &miniFrameRect);
    SDL_SetRenderDrawColor(renderer, 180, 160, 100, 255);
    SDL_RenderDrawRect(renderer, &miniFrameRect);

    // b) Vẽ bề mặt Minimap
    if (m_minimapTexture) {
        SDL_RenderCopy(renderer, m_minimapTexture, nullptr, &miniDstRect);
    }

    // c) Khung chữ nhật hiển thị tầm nhìn hiện tại của Camera (Camera Frustum)
    float camViewW = static_cast<float>(screenWidth) / zoom;
    float camViewH = static_cast<float>(screenHeight) / zoom;

    float camLeftNorm   = camera.getX() / static_cast<float>(worldWidth);
    float camTopNorm    = camera.getY() / static_cast<float>(worldHeight);
    float camWidthNorm  = camViewW / static_cast<float>(worldWidth);
    float camHeightNorm = camViewH / static_cast<float>(worldHeight);

    int camBoxX = miniDstRect.x + static_cast<int>(std::round(camLeftNorm * miniDstRect.w));
    int camBoxY = miniDstRect.y + static_cast<int>(std::round(camTopNorm * miniDstRect.h));
    int camBoxW = std::max(2, static_cast<int>(std::round(camWidthNorm * miniDstRect.w)));
    int camBoxH = std::max(2, static_cast<int>(std::round(camHeightNorm * miniDstRect.h)));

    SDL_Rect camBoxRect{camBoxX, camBoxY, camBoxW, camBoxH};

    // Vẽ khung viền camera màu vàng sáng nổi bật trên Minimap
    SDL_SetRenderDrawColor(renderer, 255, 230, 80, 255);
    SDL_RenderDrawRect(renderer, &camBoxRect);
}

} // namespace render
