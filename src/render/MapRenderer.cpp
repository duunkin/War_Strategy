#include "MapRenderer.hpp"
#include "Camera.hpp"
#include "sim/vision/VisionSystem.hpp"
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
    if (m_fogTexture) {
        SDL_DestroyTexture(m_fogTexture);
        m_fogTexture = nullptr;
    }
}

SDL_Color MapRenderer::getTerrainColor(sim::TerrainType type) {
    switch (type) {
        case sim::TerrainType::Sea:
            return SDL_Color{30, 75, 155, 255};
        case sim::TerrainType::Plain:
            return SDL_Color{115, 175, 75, 255};
        case sim::TerrainType::Forest:
            return SDL_Color{45, 110, 50, 255};
        case sim::TerrainType::Hill:
            return SDL_Color{180, 160, 95, 255};
        case sim::TerrainType::Mountain:
            return SDL_Color{140, 130, 125, 255};
        case sim::TerrainType::River:
            return SDL_Color{65, 135, 215, 255};
        case sim::TerrainType::Lake:
            return SDL_Color{50, 115, 195, 255};
        default:
            return SDL_Color{80, 80, 80, 255};
    }
}

void MapRenderer::updateFogTexture(SDL_Renderer* renderer, const std::vector<uint8_t>& fogMap, int mapW, int mapH, bool debugFogDisabled) {
    if (!renderer || mapW <= 0 || mapH <= 0) return;

    if (!m_fogTexture) {
        m_fogTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            mapW,
            mapH
        );
        if (!m_fogTexture) return;
        // Bắt buộc theo Điều chỉnh 6: Lọc tuyến tính để biên chuyển mềm mại, không giật
        SDL_SetTextureScaleMode(m_fogTexture, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(m_fogTexture, SDL_BLENDMODE_BLEND);
    }

    void* pixelsPtr = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_fogTexture, nullptr, &pixelsPtr, &pitch) == 0) {
        auto* pixelData = static_cast<uint32_t*>(pixelsPtr);
        int pitchElements = pitch / 4;

        for (int y = 0; y < mapH; ++y) {
            for (int x = 0; x < mapW; ++x) {
                size_t idx = static_cast<size_t>(y * mapW + x);
                uint8_t state = (idx < fogMap.size() && !debugFogDisabled) ? fogMap[idx] : 2;

                uint8_t r = 10;
                uint8_t g = 12;
                uint8_t b = 16;
                uint8_t a = 0;

                if (state == 0) {
                    // Unexplored: Đen tuyền 100%
                    a = 255;
                } else if (state == 1) {
                    // Explored: Tối mờ (khoảng 68% độ che phủ) để lộ địa hình mờ ảo
                    a = 175;
                } else {
                    // Visible: Hoàn toàn trong suốt (Alpha = 0)
                    a = 0;
                }

                uint32_t colorValue = (static_cast<uint32_t>(r) << 24) |
                                      (static_cast<uint32_t>(g) << 16) |
                                      (static_cast<uint32_t>(b) << 8)  |
                                      (static_cast<uint32_t>(a));
                pixelData[y * pitchElements + x] = colorValue;
            }
        }
        SDL_UnlockTexture(m_fogTexture);
    }
}

void MapRenderer::renderFog(SDL_Renderer* renderer, const Camera& camera, int screenWidth, int screenHeight, int worldWidth, int worldHeight, bool debugFogDisabled) {
    if (!renderer || !m_fogTexture || debugFogDisabled) return;

    float zoom = camera.getZoom();
    SDL_Rect dstRect{
        .x = static_cast<int>(std::round(camera.worldToScreenX(0.0f))),
        .y = static_cast<int>(std::round(camera.worldToScreenY(0.0f))),
        .w = static_cast<int>(std::round(static_cast<float>(worldWidth) * zoom)),
        .h = static_cast<int>(std::round(static_cast<float>(worldHeight) * zoom))
    };

    SDL_RenderCopy(renderer, m_fogTexture, nullptr, &dstRect);
}

void MapRenderer::updateMinimapTexture(SDL_Renderer* renderer,
                                       const sim::WorldMap& map,
                                       const std::vector<uint8_t>& fogMap,
                                       const std::vector<sim::Unit>& units,
                                       sim::Faction viewingFaction,
                                       bool debugFogDisabled) {
    if (!renderer) return;

    int mapW = map.getWidth();
    int mapH = map.getHeight();

    // Tạo texture streaming 256x192 nếu chưa có
    if (!m_minimapTexture) {
        m_minimapTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            m_minimapWidth,
            m_minimapHeight
        );
        if (!m_minimapTexture) return;
        SDL_SetTextureScaleMode(m_minimapTexture, SDL_ScaleModeNearest);
    }

    void* pixelsPtr = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(m_minimapTexture, nullptr, &pixelsPtr, &pitch) == 0) {
        auto* pixelData = static_cast<uint32_t*>(pixelsPtr);
        int pitchElements = pitch / 4;

        // 1. Vẽ nền địa hình có áp dụng trạng thái sương mù
        for (int ty = 0; ty < mapH; ++ty) {
            for (int tx = 0; tx < mapW; ++tx) {
                size_t fogIdx = static_cast<size_t>(ty * mapW + tx);
                uint8_t state = (fogIdx < fogMap.size() && !debugFogDisabled) ? fogMap[fogIdx] : 2;

                sim::TerrainType t = map.getTile(tx, ty);
                SDL_Color c = getTerrainColor(t);

                uint8_t r = c.r;
                uint8_t g = c.g;
                uint8_t b = c.b;

                if (state == 0) {
                    // Chưa khám phá: Màu đen tuyền
                    r = 12; g = 14; b = 18;
                } else if (state == 1) {
                    // Đã khám phá nhưng không thấy: Giảm 50% độ sáng
                    r /= 2; g /= 2; b /= 2;
                }

                uint32_t colorValue = (static_cast<uint32_t>(r) << 24) |
                                      (static_cast<uint32_t>(g) << 16) |
                                      (static_cast<uint32_t>(b) << 8)  |
                                      0xFF;

                // Mỗi tile là khối 2x2 pixel trên minimap 256x192
                int px = tx * 2;
                int py = ty * 2;
                pixelData[py * pitchElements + px] = colorValue;
                pixelData[py * pitchElements + (px + 1)] = colorValue;
                pixelData[(py + 1) * pitchElements + px] = colorValue;
                pixelData[(py + 1) * pitchElements + (px + 1)] = colorValue;
            }
        }

        // 2. Vẽ các chấm quân đội còn sống lên minimap
        for (const auto& u : units) {
            if (u.isDead()) continue;

            bool isFriendly = (u.getFaction() == viewingFaction);
            bool isEnemyVisible = false;

            if (!isFriendly) {
                // Kiểm tra xem quân địch có đang được phe mình nhìn thấy không
                if (debugFogDisabled || sim::VisionSystem::isUnitVisibleToFaction(u, viewingFaction, units, fogMap, mapW, mapH)) {
                    isEnemyVisible = true;
                }
            }

            if (!isFriendly && !isEnemyVisible) {
                continue; // Địch ngoài tầm nhìn hoàn toàn không hiện trên Minimap!
            }

            // Tọa độ pixel tâm trên minimap (128 ô * 1000 milli tương ứng m_minimapWidth 256 px)
            int cx = (u.getPosX() * m_minimapWidth) / (mapW * 1000);
            int cy = (u.getPosY() * m_minimapHeight) / (mapH * 1000);

            uint8_t ur = 0, ug = 0, ub = 0;
            int dotRadius = 1;

            if (u.getType() == sim::UnitType::Commander) {
                dotRadius = 2; // Tư lệnh vẽ to hơn
                if (isFriendly) {
                    ur = 255; ug = 220; ub = 60; // Vàng kim cho Tư lệnh ta
                } else {
                    ur = 255; ug = 60; ub = 60;  // Đỏ rực cho Tư lệnh địch
                }
            } else {
                dotRadius = 1;
                if (isFriendly) {
                    ur = 60; ug = 160; ub = 255; // Xanh dương cho quân ta
                } else {
                    ur = 240; ug = 50; ub = 50;  // Đỏ cho quân địch
                }
            }

            uint32_t unitColor = (static_cast<uint32_t>(ur) << 24) |
                                 (static_cast<uint32_t>(ug) << 16) |
                                 (static_cast<uint32_t>(ub) << 8)  |
                                 0xFF;

            for (int dy = -dotRadius; dy <= dotRadius; ++dy) {
                int py = cy + dy;
                if (py < 0 || py >= m_minimapHeight) continue;
                for (int dx = -dotRadius; dx <= dotRadius; ++dx) {
                    int px = cx + dx;
                    if (px < 0 || px >= m_minimapWidth) continue;
                    pixelData[py * pitchElements + px] = unitColor;
                }
            }
        }

        SDL_UnlockTexture(m_minimapTexture);
    }
}

SDL_Rect MapRenderer::getMinimapRect(int screenWidth, int screenHeight) const {
    return SDL_Rect{
        .x = screenWidth - m_minimapWidth - m_minimapMargin,
        .y = screenHeight - m_minimapHeight - m_minimapMargin,
        .w = m_minimapWidth,
        .h = m_minimapHeight
    };
}

bool MapRenderer::isMouseInsideMinimap(int mouseX, int mouseY, int screenWidth, int screenHeight) const {
    SDL_Rect miniRect = getMinimapRect(screenWidth, screenHeight);
    return (mouseX >= miniRect.x && mouseX < miniRect.x + miniRect.w &&
            mouseY >= miniRect.y && mouseY < miniRect.y + miniRect.h);
}

bool MapRenderer::handleMinimapClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, Camera& camera) const {
    SDL_Rect miniRect = getMinimapRect(screenWidth, screenHeight);
    if (mouseX < miniRect.x || mouseX >= miniRect.x + miniRect.w ||
        mouseY < miniRect.y || mouseY >= miniRect.y + miniRect.h) {
        return false;
    }

    float u = static_cast<float>(mouseX - miniRect.x) / static_cast<float>(miniRect.w);
    float v = static_cast<float>(mouseY - miniRect.y) / static_cast<float>(miniRect.h);
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float targetWorldX = u * static_cast<float>(map.getWorldWidth());
    float targetWorldY = v * static_cast<float>(map.getWorldHeight());

    float viewW = static_cast<float>(screenWidth) / camera.getZoom();
    float viewH = static_cast<float>(screenHeight) / camera.getZoom();

    camera.setPosition(targetWorldX - (viewW * 0.5f), targetWorldY - (viewH * 0.5f));
    camera.clampToBounds(screenWidth, screenHeight, map.getWorldWidth(), map.getWorldHeight());

    return true;
}

bool MapRenderer::handleMinimapRightClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, int& outTileX, int& outTileY) const {
    SDL_Rect miniRect = getMinimapRect(screenWidth, screenHeight);
    if (mouseX < miniRect.x || mouseX >= miniRect.x + miniRect.w ||
        mouseY < miniRect.y || mouseY >= miniRect.y + miniRect.h) {
        return false;
    }

    float u = static_cast<float>(mouseX - miniRect.x) / static_cast<float>(miniRect.w);
    float v = static_cast<float>(mouseY - miniRect.y) / static_cast<float>(miniRect.h);
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    outTileX = std::clamp(static_cast<int>(u * map.getWidth()), 0, map.getWidth() - 1);
    outTileY = std::clamp(static_cast<int>(v * map.getHeight()), 0, map.getHeight() - 1);
    return true;
}

void MapRenderer::renderWorldTerrain(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight) {
    if (!renderer) return;

    float zoom = camera.getZoom();
    int tileSize = map.getTileSize();
    int worldWidth = map.getWorldWidth();
    int worldHeight = map.getWorldHeight();

    // Frustum culling
    float viewLeftWorld   = std::max(0.0f, camera.getX());
    float viewRightWorld  = std::min(static_cast<float>(worldWidth), camera.getX() + (static_cast<float>(screenWidth) / zoom));
    float viewTopWorld    = std::max(0.0f, camera.getY());
    float viewBottomWorld = std::min(static_cast<float>(worldHeight), camera.getY() + (static_cast<float>(screenHeight) / zoom));

    int startCol = std::clamp(static_cast<int>(std::floor(viewLeftWorld / tileSize)), 0, map.getWidth() - 1);
    int endCol   = std::clamp(static_cast<int>(std::ceil(viewRightWorld / tileSize)), 0, map.getWidth() - 1);
    int startRow = std::clamp(static_cast<int>(std::floor(viewTopWorld / tileSize)), 0, map.getHeight() - 1);
    int endRow   = std::clamp(static_cast<int>(std::ceil(viewBottomWorld / tileSize)), 0, map.getHeight() - 1);

    int scaledTileSize = static_cast<int>(std::ceil(static_cast<float>(tileSize) * zoom));

    // Vẽ từng ô địa hình
    for (int row = startRow; row <= endRow; ++row) {
        float worldY = static_cast<float>(row * tileSize);
        int screenY = static_cast<int>(std::floor(camera.worldToScreenY(worldY)));

        for (int col = startCol; col <= endCol; ++col) {
            float worldX = static_cast<float>(col * tileSize);
            int screenX = static_cast<int>(std::floor(camera.worldToScreenX(worldX)));

            sim::TerrainType t = map.getTile(col, row);
            SDL_Color color = getTerrainColor(t);

            SDL_Rect tileRect{screenX, screenY, scaledTileSize + 1, scaledTileSize + 1};
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &tileRect);
        }
    }

    // Vẽ lưới ô (khi zoom >= 0.5)
    if (zoom >= 0.5f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, m_gridColor.r, m_gridColor.g, m_gridColor.b, m_gridColor.a);

        int lineY1 = static_cast<int>(std::round(camera.worldToScreenY(static_cast<float>(startRow * tileSize))));
        int lineY2 = static_cast<int>(std::round(camera.worldToScreenY(static_cast<float>((endRow + 1) * tileSize))));
        for (int col = startCol; col <= endCol + 1; ++col) {
            float worldX = static_cast<float>(col * tileSize);
            int screenX = static_cast<int>(std::round(camera.worldToScreenX(worldX)));
            if (screenX >= 0 && screenX <= screenWidth) {
                SDL_RenderDrawLine(renderer, screenX, lineY1, screenX, lineY2);
            }
        }

        int lineX1 = static_cast<int>(std::round(camera.worldToScreenX(static_cast<float>(startCol * tileSize))));
        int lineX2 = static_cast<int>(std::round(camera.worldToScreenX(static_cast<float>((endCol + 1) * tileSize))));
        for (int row = startRow; row <= endRow + 1; ++row) {
            float worldY = static_cast<float>(row * tileSize);
            int screenY = static_cast<int>(std::round(camera.worldToScreenY(worldY)));
            if (screenY >= 0 && screenY <= screenHeight) {
                SDL_RenderDrawLine(renderer, lineX1, screenY, lineX2, screenY);
            }
        }
    }

    // Đường viền ranh giới thế giới
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
}

void MapRenderer::renderMinimap(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight) {
    if (!renderer) return;

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
    float zoom = camera.getZoom();
    float camViewW = static_cast<float>(screenWidth) / zoom;
    float camViewH = static_cast<float>(screenHeight) / zoom;

    float camLeftNorm   = camera.getX() / static_cast<float>(map.getWorldWidth());
    float camTopNorm    = camera.getY() / static_cast<float>(map.getWorldHeight());
    float camWidthNorm  = camViewW / static_cast<float>(map.getWorldWidth());
    float camHeightNorm = camViewH / static_cast<float>(map.getWorldHeight());

    int camBoxX = miniDstRect.x + static_cast<int>(std::round(camLeftNorm * miniDstRect.w));
    int camBoxY = miniDstRect.y + static_cast<int>(std::round(camTopNorm * miniDstRect.h));
    int camBoxW = std::max(2, static_cast<int>(std::round(camWidthNorm * miniDstRect.w)));
    int camBoxH = std::max(2, static_cast<int>(std::round(camHeightNorm * miniDstRect.h)));

    SDL_Rect camBoxRect{camBoxX, camBoxY, camBoxW, camBoxH};
    SDL_SetRenderDrawColor(renderer, 255, 230, 80, 255);
    SDL_RenderDrawRect(renderer, &camBoxRect);
}

} // namespace render
