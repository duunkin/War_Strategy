#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include "sim/map/WorldMap.hpp"

namespace render {
class Camera;

/**
 * @brief Lớp MapRenderer chịu trách nhiệm vẽ bản đồ thế giới và hệ thống minimap.
 *
 * Tính năng:
 * - Tô màu từng ô theo loại địa hình (Biển, Đất bằng, Đồi, Núi, Rừng, Sông, Hồ).
 * - Frustum culling: chỉ tính toán và vẽ các ô nằm trong khung nhìn camera.
 * - Vẽ đường viền bản đồ thế giới và lưới ô.
 * - Vẽ Minimap ở góc dưới phải màn hình hiển thị toàn bộ địa hình thế giới.
 * - Vẽ khung chữ nhật thể hiện vùng nhìn camera trên Minimap.
 * - Hỗ trợ bấm / kéo chuột trên Minimap để dịch chuyển tức thời vị trí camera.
 */
class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer();

    // Vẽ bản đồ và minimap lên SDL_Renderer
    void render(SDL_Renderer* renderer, const sim::WorldMap& map, const Camera& camera, int screenWidth, int screenHeight);

    // Cập nhật lại bề mặt texture của minimap khi sinh bản đồ mới
    void updateMinimapTexture(SDL_Renderer* renderer, const sim::WorldMap& map);

    // Trả về vùng hình chữ nhật của Minimap trên màn hình
    [[nodiscard]] SDL_Rect getMinimapRect(int screenWidth, int screenHeight) const;

    // Xử lý sự kiện nhấn/kéo chuột trên Minimap; trả về true nếu chuột nằm trong Minimap và đã dịch chuyển camera
    bool handleMinimapClick(int mouseX, int mouseY, int screenWidth, int screenHeight, const sim::WorldMap& map, Camera& camera) const;

    // Giải phóng tài nguyên texture
    void cleanup();

    // Lấy mã màu SDL_Color đại diện cho một loại địa hình
    static SDL_Color getTerrainColor(sim::TerrainType type);

private:
    SDL_Texture* m_minimapTexture{nullptr};
    uint64_t     m_lastMapHash{0}; // Dùng để tự động cập nhật minimap nếu bản đồ đổi

    // Kích thước minimap hiển thị ở góc dưới phải (tỉ lệ 4:3 khớp 128x96)
    int m_minimapWidth{256};  // 128 ô * 2 pixel
    int m_minimapHeight{192}; // 96 ô * 2 pixel
    int m_minimapMargin{16};  // Cách lề phải và lề dưới

    // Màu đường kẻ lưới ô và viền
    SDL_Color m_gridColor{40, 44, 52, 160};
    SDL_Color m_borderColor{210, 180, 110, 255};
};

} // namespace render
