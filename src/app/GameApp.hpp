#pragma once

#include "../render/Window.hpp"
#include "../render/Camera.hpp"
#include "../render/MapRenderer.hpp"
#include "../render/TextRenderer.hpp"
#include "../sim/map/WorldMap.hpp"

namespace app {

/**
 * @brief Lớp GameApp quản lý vòng đời ứng dụng, vòng lặp game chính, tiếp nhận sự kiện và vẽ khung hình.
 */
class GameApp {
public:
    GameApp();
    ~GameApp();

    // Chạy vòng lặp game chính, trả về mã thoát (0 nếu thành công)
    int run();

private:
    bool init();
    void processEvents();
    void update(float deltaTime);
    void render();
    void cleanup();

private:
    bool m_running{false};

    render::Window       m_window;
    render::Camera       m_camera;
    render::MapRenderer  m_mapRenderer;
    render::TextRenderer m_textRenderer;
    sim::WorldMap        m_worldMap{128, 96, 32}; // 128x96 ô, mỗi ô 32x32

    // Biến đo lường và tính toán FPS
    int   m_frameCount{0};
    float m_fpsAccumulator{0.0f};
    float m_currentFPS{0.0f};
};

} // namespace app
