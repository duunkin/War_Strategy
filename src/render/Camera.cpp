#include "Camera.hpp"
#include <algorithm>
#include <cmath>

namespace render {

Camera::Camera() = default;

void Camera::update(float deltaTime, int screenWidth, int screenHeight, int worldWidth, int worldHeight) {
    float moveX = 0.0f;
    float moveY = 0.0f;

    // 1. Kiểm tra trạng thái bàn phím (WASD hoặc phím mũi tên)
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    if (keyState[SDL_SCANCODE_W] || keyState[SDL_SCANCODE_UP]) {
        moveY -= 1.0f;
    }
    if (keyState[SDL_SCANCODE_S] || keyState[SDL_SCANCODE_DOWN]) {
        moveY += 1.0f;
    }
    if (keyState[SDL_SCANCODE_A] || keyState[SDL_SCANCODE_LEFT]) {
        moveX -= 1.0f;
    }
    if (keyState[SDL_SCANCODE_D] || keyState[SDL_SCANCODE_RIGHT]) {
        moveX += 1.0f;
    }

    // 2. Kiểm tra đưa chuột ra mép màn hình (Edge scrolling)
    int mouseX = 0;
    int mouseY = 0;
    Uint32 mouseFlags = SDL_GetMouseState(&mouseX, &mouseY);
    (void)mouseFlags; // Tránh cảnh báo unused

    // Chỉ kích hoạt edge scrolling khi chuột nằm trong phạm vi hiển thị của cửa sổ
    if (mouseX >= 0 && mouseX < screenWidth && mouseY >= 0 && mouseY < screenHeight) {
        if (mouseX <= m_edgeMargin) {
            moveX -= 1.0f;
        } else if (mouseX >= screenWidth - m_edgeMargin) {
            moveX += 1.0f;
        }

        if (mouseY <= m_edgeMargin) {
            moveY -= 1.0f;
        } else if (mouseY >= screenHeight - m_edgeMargin) {
            moveY += 1.0f;
        }
    }

    // Chuẩn hóa vector di chuyển nếu đi chéo để tốc độ không bị nhanh hơn
    if (moveX != 0.0f && moveY != 0.0f) {
        constexpr float invSqrt2 = 0.70710678f;
        moveX *= invSqrt2;
        moveY *= invSqrt2;
    }

    // Di chuyển camera: vận tốc trong thế giới = tốc độ pixel màn hình chia cho mức zoom
    if (moveX != 0.0f || moveY != 0.0f) {
        float worldSpeed = m_moveSpeed / m_zoom;
        m_x += moveX * worldSpeed * deltaTime;
        m_y += moveY * worldSpeed * deltaTime;
    }

    // 3. Giới hạn camera không ra ngoài bản đồ
    clampToBounds(screenWidth, screenHeight, worldWidth, worldHeight);
}

void Camera::handleEvent(const SDL_Event& event, int screenWidth, int screenHeight, int worldWidth, int worldHeight) {
    // Xử lý zoom bằng con lăn chuột (SDL_MOUSEWHEEL)
    if (event.type == SDL_MOUSEWHEEL) {
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);

        // Vị trí điểm thế giới tại con trỏ trước khi zoom
        float worldMouseBeforeX = screenToWorldX(static_cast<float>(mouseX));
        float worldMouseBeforeY = screenToWorldY(static_cast<float>(mouseY));

        // Tăng giảm hệ số zoom theo chiều lăn chuột
        float zoomFactor = (event.wheel.y > 0) ? 1.15f : 0.87f;
        float newZoom = std::clamp(m_zoom * zoomFactor, m_minZoom, m_maxZoom);

        if (newZoom != m_zoom) {
            m_zoom = newZoom;

            // Dịch chuyển camera để điểm thế giới dưới con trỏ giữ nguyên vị trí trên màn hình
            m_x = worldMouseBeforeX - (static_cast<float>(mouseX) / m_zoom);
            m_y = worldMouseBeforeY - (static_cast<float>(mouseY) / m_zoom);

            clampToBounds(screenWidth, screenHeight, worldWidth, worldHeight);
        }
    }
}

void Camera::clampToBounds(int screenWidth, int screenHeight, int worldWidth, int worldHeight) {
    // Chiều rộng và chiều cao vùng nhìn thấy trong thế giới
    float viewW = static_cast<float>(screenWidth) / m_zoom;
    float viewH = static_cast<float>(screenHeight) / m_zoom;
    float totalW = static_cast<float>(worldWidth);
    float totalH = static_cast<float>(worldHeight);

    // Xử lý trục X
    if (viewW < totalW) {
        // Vùng nhìn nhỏ hơn bản đồ: ép trong khoảng [0, totalW - viewW]
        m_x = std::clamp(m_x, 0.0f, totalW - viewW);
    } else {
        // Nếu zoom nhỏ khiến bản đồ nhỏ hơn màn hình: căn giữa theo trục ngang
        m_x = (totalW - viewW) * 0.5f;
    }

    // Xử lý trục Y
    if (viewH < totalH) {
        // Vùng nhìn nhỏ hơn bản đồ: ép trong khoảng [0, totalH - viewH]
        m_y = std::clamp(m_y, 0.0f, totalH - viewH);
    } else {
        // Nếu zoom nhỏ khiến bản đồ nhỏ hơn màn hình: căn giữa theo trục dọc
        m_y = (totalH - viewH) * 0.5f;
    }
}

float Camera::worldToScreenX(float worldX) const {
    return (worldX - m_x) * m_zoom;
}

float Camera::worldToScreenY(float worldY) const {
    return (worldY - m_y) * m_zoom;
}

float Camera::screenToWorldX(float screenX) const {
    return m_x + (screenX / m_zoom);
}

float Camera::screenToWorldY(float screenY) const {
    return m_y + (screenY / m_zoom);
}

float Camera::getX() const {
    return m_x;
}

float Camera::getY() const {
    return m_y;
}

float Camera::getZoom() const {
    return m_zoom;
}

void Camera::setPosition(float x, float y) {
    m_x = x;
    m_y = y;
}

void Camera::setZoom(float zoom) {
    m_zoom = std::clamp(zoom, m_minZoom, m_maxZoom);
}

} // namespace render
