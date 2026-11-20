#include "UnitRenderer.hpp"
#include "Camera.hpp"
#include "TextRenderer.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace render {

void UnitRenderer::render(SDL_Renderer* renderer,
                         const std::vector<sim::Unit>& units,
                         const Camera& camera,
                         int screenWidth,
                         int screenHeight,
                         int tilePixels) {
    if (!renderer) return;

    float zoom = camera.getZoom();
    float pixelScale = static_cast<float>(tilePixels > 0 ? tilePixels : 32);

    // =========================================================================
    // 1. Vẽ các đường đi qua waypoints cho các chấm đang được chọn có lệnh di chuyển
    // =========================================================================
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& unit : units) {
        if (unit.isDead() || !unit.isSelected() || !unit.hasPath()) {
            continue;
        }

        const auto& waypoints = unit.getWaypoints();
        if (waypoints.empty()) continue;

        // Điểm đầu là vị trí màn hình hiện tại của đơn vị
        float unitWorldX = static_cast<float>(unit.getPosX()) * pixelScale / 1000.0f;
        float unitWorldY = static_cast<float>(unit.getPosY()) * pixelScale / 1000.0f;
        float prevScreenX = camera.worldToScreenX(unitWorldX);
        float prevScreenY = camera.worldToScreenY(unitWorldY);

        // Màu đường đi: Màu Cyan neon thanh mảnh, khác với màu phe Xanh/Đỏ
        SDL_SetRenderDrawColor(renderer, 70, 220, 245, 220);

        // Nối từ vị trí hiện tại qua các waypoint còn lại
        for (size_t wi = unit.getCurrentWaypointIndex(); wi < waypoints.size(); ++wi) {
            float wpWorldX = static_cast<float>(waypoints[wi].x) * pixelScale + pixelScale * 0.5f;
            float wpWorldY = static_cast<float>(waypoints[wi].y) * pixelScale + pixelScale * 0.5f;
            float swX = camera.worldToScreenX(wpWorldX);
            float swY = camera.worldToScreenY(wpWorldY);

            SDL_RenderDrawLine(renderer,
                               static_cast<int>(prevScreenX),
                               static_cast<int>(prevScreenY),
                               static_cast<int>(swX),
                               static_cast<int>(swY));
            prevScreenX = swX;
            prevScreenY = swY;
        }

        // Vẽ dấu đích đến ở waypoint cuối cùng
        sim::TileCoord finalTile = unit.getFinalTargetTile();
        float finalWorldX = static_cast<float>(finalTile.x) * pixelScale + pixelScale * 0.5f;
        float finalWorldY = static_cast<float>(finalTile.y) * pixelScale + pixelScale * 0.5f;
        int destScreenX = static_cast<int>(camera.worldToScreenX(finalWorldX));
        int destScreenY = static_cast<int>(camera.worldToScreenY(finalWorldY));

        if (unit.isDestinationAdjusted()) {
            // Dấu cảnh báo đích đã được điều chỉnh: Hình thoi + vòng tròn màu Cam/Đỏ cảnh báo
            SDL_SetRenderDrawColor(renderer, 255, 120, 40, 255);
            SDL_RenderDrawLine(renderer, destScreenX - 6, destScreenY, destScreenX, destScreenY - 6);
            SDL_RenderDrawLine(renderer, destScreenX, destScreenY - 6, destScreenX + 6, destScreenY);
            SDL_RenderDrawLine(renderer, destScreenX + 6, destScreenY, destScreenX, destScreenY + 6);
            SDL_RenderDrawLine(renderer, destScreenX, destScreenY + 6, destScreenX - 6, destScreenY);

            SDL_Rect innerDot{destScreenX - 2, destScreenY - 2, 5, 5};
            SDL_RenderFillRect(renderer, &innerDot);
        } else {
            // Dấu đích bình thường: Dấu chữ thập và vòng tròn nhỏ màu Cyan
            SDL_SetRenderDrawColor(renderer, 70, 230, 250, 240);
            SDL_RenderDrawLine(renderer, destScreenX - 5, destScreenY, destScreenX + 5, destScreenY);
            SDL_RenderDrawLine(renderer, destScreenX, destScreenY - 5, destScreenX, destScreenY + 5);
            SDL_Rect markerRect{destScreenX - 3, destScreenY - 3, 7, 7};
            SDL_RenderDrawRect(renderer, &markerRect);
        }
    }

    // =========================================================================
    // 2. Vẽ thân các chấm đơn vị, vòng sáng chọn và thanh máu
    // =========================================================================
    for (const auto& unit : units) {
        if (unit.isDead()) continue;

        // Chuyển đổi mili-ô sang pixel thế giới: 1 ô = 1000 mili-ô = tilePixels (32 px)
        float worldX = static_cast<float>(unit.getPosX()) * pixelScale / 1000.0f;
        float worldY = static_cast<float>(unit.getPosY()) * pixelScale / 1000.0f;

        // Chuyển sang tọa độ màn hình
        float screenX = camera.worldToScreenX(worldX);
        float screenY = camera.worldToScreenY(worldY);

        // Kích thước chấm: cơ sở 65% ô, nhân với iconSizeTiles và zoom
        float sizeFactor = (unit.getType() == sim::UnitType::Commander) ? 0.95f : 0.65f;
        float baseSize = sizeFactor * pixelScale * zoom;
        if (baseSize < 6.0f) baseSize = 6.0f;

        // Frustum culling với biên độ an toàn
        float totalH = baseSize + 14.0f;
        if (screenX + baseSize < 0.0f || screenX - baseSize > static_cast<float>(screenWidth) ||
            screenY + totalH < 0.0f || screenY - baseSize > static_cast<float>(screenHeight)) {
            continue;
        }

        int rx = static_cast<int>(screenX - baseSize * 0.5f);
        int ry = static_cast<int>(screenY - baseSize * 0.5f);
        int rsize = static_cast<int>(baseSize);

        SDL_Rect rect{rx, ry, rsize, rsize};

        // Vòng sáng nổi bật cho chấm đang được chọn
        if (unit.isSelected()) {
            SDL_Rect halo1{rx - 2, ry - 2, rsize + 4, rsize + 4};
            SDL_SetRenderDrawColor(renderer, 255, 235, 75, 255);
            SDL_RenderDrawRect(renderer, &halo1);
            SDL_Rect halo2{rx - 3, ry - 3, rsize + 6, rsize + 6};
            SDL_SetRenderDrawColor(renderer, 255, 210, 40, 180);
            SDL_RenderDrawRect(renderer, &halo2);
        }

        // Tô màu thân đơn vị theo phe (Player: Xanh dương, Enemy: Đỏ cam)
        if (unit.getFaction() == sim::Faction::Player) {
            SDL_SetRenderDrawColor(renderer, 35, 120, 235, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 225, 45, 45, 255);
        }
        SDL_RenderFillRect(renderer, &rect);

        // Viền ngoài của chấm
        if (unit.isSelected()) {
            SDL_SetRenderDrawColor(renderer, 255, 245, 120, 255);
            SDL_RenderDrawRect(renderer, &rect);
        } else {
            if (unit.getFaction() == sim::Faction::Player) {
                SDL_SetRenderDrawColor(renderer, 240, 245, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 190, 190, 255);
            }
            SDL_RenderDrawRect(renderer, &rect);
        }

        // Biểu tượng lõi trung tâm màu vàng kim cho Tư lệnh (Commander)
        if (unit.getType() == sim::UnitType::Commander) {
            int coreInset = std::max(2, rsize / 4);
            SDL_Rect coreRect{rx + coreInset, ry + coreInset, rsize - coreInset * 2, rsize - coreInset * 2};
            SDL_SetRenderDrawColor(renderer, 255, 225, 60, 255);
            SDL_RenderFillRect(renderer, &coreRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &coreRect);
        }

        // Thanh máu (Health bar) phía trên chấm
        int barH = std::max(3, static_cast<int>(baseSize * 0.20f));
        int barW = rsize;
        int barX = rx;
        int barY = ry - barH - 3;

        SDL_Rect barBg{barX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 220);
        SDL_RenderFillRect(renderer, &barBg);

        float hpRatio = 0.0f;
        if (unit.getMaxHp() > 0) {
            hpRatio = static_cast<float>(unit.getHp()) / static_cast<float>(unit.getMaxHp());
            hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);
        }
        int fillW = static_cast<int>(static_cast<float>(barW) * hpRatio);
        if (fillW > 0) {
            SDL_Rect barFill{barX, barY, fillW, barH};
            if (unit.getFaction() == sim::Faction::Player) {
                if (hpRatio > 0.5f) {
                    SDL_SetRenderDrawColor(renderer, 50, 225, 90, 255); // Xanh lá
                } else if (hpRatio > 0.25f) {
                    SDL_SetRenderDrawColor(renderer, 240, 210, 40, 255); // Vàng
                } else {
                    SDL_SetRenderDrawColor(renderer, 240, 50, 40, 255);  // Đỏ
                }
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 75, 65, 255); // Đỏ tươi
            }
            SDL_RenderFillRect(renderer, &barFill);
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderDrawRect(renderer, &barBg);
    }
}

void UnitRenderer::renderSelectionBox(SDL_Renderer* renderer, int startX, int startY, int currentX, int currentY) {
    if (!renderer) return;

    int minX = std::min(startX, currentX);
    int maxX = std::max(startX, currentX);
    int minY = std::min(startY, currentY);
    int maxY = std::max(startY, currentY);

    SDL_Rect boxRect{minX, minY, maxX - minX, maxY - minY};
    if (boxRect.w > 0 && boxRect.h > 0) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // Nền chữ nhật mờ màu xanh
        SDL_SetRenderDrawColor(renderer, 40, 140, 240, 50);
        SDL_RenderFillRect(renderer, &boxRect);
        // Viền sáng rõ nét
        SDL_SetRenderDrawColor(renderer, 90, 210, 255, 230);
        SDL_RenderDrawRect(renderer, &boxRect);
    }
}

void UnitRenderer::renderInfoPanel(SDL_Renderer* renderer,
                                  const sim::Unit& unit,
                                  TextRenderer& textRenderer,
                                  int screenWidth,
                                  int screenHeight) {
    if (!renderer) return;

    int panelW = 300;
    int panelH = 118;
    int panelX = 16;
    int panelY = screenHeight - panelH - 16;

    SDL_Rect bgRect{panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 14, 18, 26, 235);
    SDL_RenderFillRect(renderer, &bgRect);

    // Viền bảng theo phe
    if (unit.getFaction() == sim::Faction::Player) {
        SDL_SetRenderDrawColor(renderer, 60, 150, 240, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 235, 65, 65, 255);
    }
    SDL_RenderDrawRect(renderer, &bgRect);

    // Tiêu đề: Tên đơn vị và phe
    std::string factionStr = (unit.getFaction() == sim::Faction::Player) ? "[Phe Ta]" : "[Quan Dich]";
    std::string title = unit.getName() + " " + factionStr;
    SDL_Color titleColor = (unit.getFaction() == sim::Faction::Player) ? SDL_Color{120, 200, 255, 255} : SDL_Color{255, 140, 130, 255};
    textRenderer.renderText(renderer, title, panelX + 12, panelY + 10, titleColor, false);

    // Dòng 1: Quân số trong chấm
    char line1[64];
    std::snprintf(line1, sizeof(line1), "Quan so: %d / %d", unit.getCurrentUnits(), unit.getMaxUnits());
    SDL_Color yellowColor{255, 225, 90, 255};
    textRenderer.renderText(renderer, line1, panelX + 12, panelY + 34, yellowColor, false);

    // Dòng 2: Máu hiện tại trên tối đa
    char line2[64];
    std::snprintf(line2, sizeof(line2), "Mau: %d / %d", unit.getHp(), unit.getMaxHp());
    SDL_Color greenColor{90, 240, 130, 255};
    textRenderer.renderText(renderer, line2, panelX + 12, panelY + 58, greenColor, false);

    // Dòng 3: Tốc độ và giáp
    char line3[64];
    std::snprintf(line3, sizeof(line3), "Toc do: %.2f o/s  |  Giap: %d", unit.getSpeedTilesPerSec(), unit.getArmor());
    SDL_Color cyanColor{130, 225, 255, 255};
    textRenderer.renderText(renderer, line3, panelX + 12, panelY + 82, cyanColor, false);
}

void UnitRenderer::renderTracers(SDL_Renderer* renderer,
                                 const std::vector<sim::Tracer>& tracers,
                                 const Camera& camera,
                                 int screenWidth,
                                 int screenHeight,
                                 int tilePixels) {
    if (!renderer || tracers.empty()) return;

    float pixelScale = static_cast<float>(tilePixels);

    for (const auto& t : tracers) {
        float startWx = static_cast<float>(t.startPosX) * pixelScale / 1000.0f;
        float startWy = static_cast<float>(t.startPosY) * pixelScale / 1000.0f;
        float endWx = static_cast<float>(t.endPosX) * pixelScale / 1000.0f;
        float endWy = static_cast<float>(t.endPosY) * pixelScale / 1000.0f;

        float sx1 = camera.worldToScreenX(startWx);
        float sy1 = camera.worldToScreenY(startWy);
        float sx2 = camera.worldToScreenX(endWx);
        float sy2 = camera.worldToScreenY(endWy);

        float minX = std::min(sx1, sx2);
        float maxX = std::max(sx1, sx2);
        float minY = std::min(sy1, sy2);
        float maxY = std::max(sy1, sy2);
        if (maxX < 0 || minX > screenWidth || maxY < 0 || minY > screenHeight) {
            continue;
        }

        int x1 = static_cast<int>(sx1);
        int y1 = static_cast<int>(sy1);
        int x2 = static_cast<int>(sx2);
        int y2 = static_cast<int>(sy2);

        if (t.ammoType == "HE") {
            SDL_SetRenderDrawColor(renderer, 255, 140, 30, 240);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            SDL_SetRenderDrawColor(renderer, 255, 230, 100, 255);
            SDL_RenderDrawLine(renderer, x1 + 1, y1, x2 + 1, y2);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 245, 140, 240);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            SDL_SetRenderDrawColor(renderer, 255, 255, 220, 255);
            SDL_RenderDrawLine(renderer, x1, y1 + 1, x2, y2 + 1);
        }

        SDL_Rect impactDot{x2 - 2, y2 - 2, 5, 5};
        SDL_SetRenderDrawColor(renderer, 255, 255, 200, 255);
        SDL_RenderFillRect(renderer, &impactDot);
    }
}

} // namespace render
