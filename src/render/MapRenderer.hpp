#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "sim/map/WorldMap.hpp"
#include "sim/units/Unit.hpp"
#include <vector>
#include <cstdint>

namespace render {
class Camera;

/**
 * @brief Lớp MapRenderer chịu trách nhiệm vẽ bản đồ thế giới, sương mù chiến tranh và hệ thống minimap.
 */
class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer();

    // Vẽ bề mặt địa hình thế giới và lưới ô
    void renderWorldTerrain(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight);

    // Cập nhật texture sương mù 128x96
    void updateFogTexture(SDL_Renderer* renderer, const std::vector<uint8_t>& fogMap, int mapW, int mapH, bool debugFogDisabled);

    // Vẽ lớp phủ sương mù chiến tranh lên bản đồ thế giới bằng texture 128x96 phóng to có lọc tuyến tính (M6)
    void renderFog(SDL_Renderer* renderer, const Camera& camera, int screenWidth, int screenHeight, int worldWidth, int worldHeight, bool debugFogDisabled);

    // Cập nhật bề mặt texture của minimap (chỉ cập nhật theo tick sim)
    void updateMinimapTexture(SDL_Renderer* renderer,
                              const sim::WorldMap& map,
                              const std::vector<uint8_t>& fogMap,
                              const std::vector<sim::Unit>& units,
                              sim::Faction viewingFaction,
                              bool debugFogDisabled);

    // Vẽ Minimap ở góc dưới phải màn hình
    void renderMinimap(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight);

    // Trả về vùng hình chữ nhật của Minimap trên màn hình
    [[nodiscard]] SDL_Rect getMinimapRect(int screenWidth, int screenHeight) const;

    // Kiểm tra chuột có nằm trong vùng Minimap hay không
    [[nodiscard]] bool isMouseInsideMinimap(int mouseX, int mouseY, int screenWidth, int screenHeight) const;

    // Xử lý sự kiện nhấn/kéo chuột trái trên Minimap (chỉ dịch chuyển camera)
    bool handleMinimapClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, Camera& camera) const;

    // Xử lý click chuột phải trên Minimap (ra lệnh di chuyển tới ô tương ứng)
    bool handleMinimapRightClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, int& outTileX, int& outTileY) const;

    // Giải phóng tài nguyên texture
    void cleanup();

    // Lấy mã màu SDL_Color đại diện cho một loại địa hình
    static SDL_Color getTerrainColor(sim::TerrainType type);

private:
    SDL_Texture* m_minimapTexture{nullptr};
    SDL_Texture* m_fogTexture{nullptr};
    uint64_t     m_lastMapHash{0};

    // Kích thước minimap hiển thị ở góc dưới phải (tỉ lệ 4:3 khớp 128x96, mỗi ô 2x2 px)
    int m_minimapWidth{256};
    int m_minimapHeight{192};
    int m_minimapMargin{16};

    // Màu đường kẻ lưới ô và viền
    SDL_Color m_gridColor{40, 44, 52, 160};
    SDL_Color m_borderColor{210, 180, 110, 255};
};

} // namespace render
