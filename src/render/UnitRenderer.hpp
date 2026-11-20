#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "../sim/units/Unit.hpp"
#include "../sim/combat/CombatSystem.hpp"
#include <vector>
#include <functional>

namespace render {
class Camera;
class TextRenderer;

/**
 * @brief Lớp UnitRenderer vẽ các chấm quân đội, vòng chọn sáng, đường đi A*, đường đạn và bảng thông tin.
 */
class UnitRenderer {
public:
    UnitRenderer() = default;
    ~UnitRenderer() = default;

    // Vẽ toàn bộ đơn vị và các đường đi của các đơn vị đang chọn (lọc theo tầm nhìn sương mù - M6)
    void render(SDL_Renderer* renderer,
                const std::vector<sim::Unit>& units,
                const Camera& camera,
                int screenWidth,
                int screenHeight,
                const std::function<bool(const sim::Unit&)>& isUnitVisibleFunc,
                sim::Faction viewingFaction,
                bool debugFogDisabled,
                int tilePixels = 32);

    // Vẽ các đường đạn đang hoạt động trên chiến trường (lọc theo tầm nhìn sương mù - M6)
    void renderTracers(SDL_Renderer* renderer,
                       const std::vector<sim::Tracer>& tracers,
                       const Camera& camera,
                       int screenWidth,
                       int screenHeight,
                       const std::function<bool(int, int)>& isTileVisibleFunc,
                       bool debugFogDisabled,
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
