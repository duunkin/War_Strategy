#include "UnitRenderer.hpp"
#include "Camera.hpp"
#include <algorithm>

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

        // Frustum culling với biên độ an toàn cho thanh máu phía trên
        float totalH = baseSize + 12.0f;
        if (screenX + baseSize < 0.0f || screenX - baseSize > static_cast<float>(screenWidth) ||
            screenY + totalH < 0.0f || screenY - baseSize > static_cast<float>(screenHeight)) {
            continue;
        }

        int rx = static_cast<int>(screenX - baseSize * 0.5f);
        int ry = static_cast<int>(screenY - baseSize * 0.5f);
        int rsize = static_cast<int>(baseSize);

        SDL_Rect rect{rx, ry, rsize, rsize};

        // 1. Tô màu thân đơn vị theo phe (Player: Xanh dương, Enemy: Đỏ cam)
        if (unit.getFaction() == sim::Faction::Player) {
            SDL_SetRenderDrawColor(renderer, 35, 120, 235, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 225, 45, 45, 255);
        }
        SDL_RenderFillRect(renderer, &rect);

        // 2. Viền ngoài
        if (unit.isSelected()) {
            // Đang được chọn: Viền vàng sáng 2 lớp
            SDL_SetRenderDrawColor(renderer, 255, 230, 70, 255);
            SDL_RenderDrawRect(renderer, &rect);
            SDL_Rect outerRect{rx - 1, ry - 1, rsize + 2, rsize + 2};
            SDL_RenderDrawRect(renderer, &outerRect);
        } else {
            // Viền sáng tiêu chuẩn
            if (unit.getFaction() == sim::Faction::Player) {
                SDL_SetRenderDrawColor(renderer, 240, 245, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 190, 190, 255);
            }
            SDL_RenderDrawRect(renderer, &rect);
        }

        // 3. Nếu là Tư lệnh (Commander): vẽ biểu tượng lõi trung tâm màu vàng kim
        if (unit.getType() == sim::UnitType::Commander) {
            int coreInset = std::max(2, rsize / 4);
            SDL_Rect coreRect{rx + coreInset, ry + coreInset, rsize - coreInset * 2, rsize - coreInset * 2};
            SDL_SetRenderDrawColor(renderer, 255, 225, 60, 255);
            SDL_RenderFillRect(renderer, &coreRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &coreRect);
        }

        // 4. Vẽ thanh máu (Health bar) phía trên chấm
        int barH = std::max(3, static_cast<int>(baseSize * 0.20f));
        int barW = rsize;
        int barX = rx;
        int barY = ry - barH - 3;

        // Nền đen mờ của thanh máu
        SDL_Rect barBg{barX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 220);
        SDL_RenderFillRect(renderer, &barBg);

        // Ruột máu theo tỉ lệ hp / maxHp
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

        // Viền thanh máu
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderDrawRect(renderer, &barBg);
    }
}

} // namespace render
