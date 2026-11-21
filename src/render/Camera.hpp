#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

namespace render {

/**
 * @brief Lớp Camera quản lý góc nhìn, vị trí (x, y) và mức độ thu phóng (zoom).
 *
 * Chức năng:
 * - Cuộn bản đồ bằng phím mũi tên hoặc phím W, A, S, D.
 * - Cuộn bản đồ khi rê chuột đến sát mép màn hình (edge scrolling).
 * - Thu phóng (zoom in/out) mượt mà bằng con lăn chuột hướng vào vị trí con trỏ.
 * - Giới hạn vùng nhìn (clamp) không cho camera vượt ra khỏi bản đồ.
 */
class Camera {
public:
    Camera();
    ~Camera() = default;

    // Cập nhật vị trí camera dựa trên phím bấm và vị trí chuột theo thời gian thực (deltaTime tính bằng giây)
    void update(float deltaTime, int screenWidth, int screenHeight, int worldWidth, int worldHeight);

    // Xử lý sự kiện con lăn chuột để zoom
    void handleEvent(const SDL_Event& event, int screenWidth, int screenHeight, int worldWidth, int worldHeight);

    // Ép vị trí camera không vượt ra ngoài biên bản đồ thế giới
    void clampToBounds(int screenWidth, int screenHeight, int worldWidth, int worldHeight);

    // Chuyển đổi tọa độ thế giới (world) sang tọa độ màn hình (screen)
    [[nodiscard]] float worldToScreenX(float worldX) const;
    [[nodiscard]] float worldToScreenY(float worldY) const;

    // Chuyển đổi tọa độ màn hình (screen) sang tọa độ thế giới (world)
    [[nodiscard]] float screenToWorldX(float screenX) const;
    [[nodiscard]] float screenToWorldY(float screenY) const;

    [[nodiscard]] float getX() const;
    [[nodiscard]] float getY() const;
    [[nodiscard]] float getZoom() const;

    // Đặt vị trí camera (góc trên-trái trong tọa độ thế giới)
    void setPosition(float x, float y);

    // Đặt mức zoom
    void setZoom(float zoom);

private:
    float m_x{0.0f};          // Tọa độ X góc trên-trái camera (trong pixel thế giới)
    float m_y{0.0f};          // Tọa độ Y góc trên-trái camera (trong pixel thế giới)
    float m_zoom{1.0f};       // Tỉ lệ hiển thị (1.0 = kích thước gốc 32x32)

    float m_minZoom{0.35f};   // Mức thu nhỏ tối đa (zoom out)
    float m_maxZoom{2.5f};    // Mức phóng to tối đa (zoom in)
    float m_moveSpeed{800.0f}; // Tốc độ di chuyển cơ bản (pixel màn hình / giây)
    int   m_edgeMargin{20};   // Khoảng cách mép màn hình (pixel) để kích hoạt cuộn chuột
};

} // namespace render
