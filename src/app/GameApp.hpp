#pragma once

#include "../render/Window.hpp"
#include "../render/Camera.hpp"
#include "../render/MapRenderer.hpp"
#include "../render/TextRenderer.hpp"
#include "../sim/map/WorldMap.hpp"
#include "../mapgen/MapGenerator.hpp"
#include "../mapgen/MapGenConfig.hpp"

#include <cstdint>
#include <string>

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

    // Sinh lại bản đồ theo seed mới
    void regenerateMap(uint64_t seed);

    // Tạo một seed ngẫu nhiên từ đồng hồ hệ thống và bộ sinh số
    uint64_t generateRandomSeed();

    // Vùng chữ nhật của nút "Tao ban do moi" ở góc trên bên phải
    [[nodiscard]] SDL_Rect getNewMapButtonRect(int screenWidth) const;

private:
    bool init();
    void processEvents();
    void update(float deltaTime);
    void render();
    void cleanup();

private:
    bool m_running{false};

    render::Window         m_window;
    render::Camera         m_camera;
    render::MapRenderer    m_mapRenderer;
    render::TextRenderer   m_textRenderer;
    sim::WorldMap          m_worldMap{128, 96, 32}; // 128x96 ô, mỗi ô 32x32

    mapgen::MapGenerator   m_mapGenerator;
    mapgen::MapGenConfig   m_mapConfig;
    uint64_t               m_currentSeed{12345678ULL};

    bool                   m_isDraggingMinimap{false};

    // Thông báo hiển thị tạm trên màn hình (ví dụ: "Seed moi: <seed>")
    std::string            m_notificationText;
    float                  m_notificationTimer{0.0f};

    // Biến đo lường và tính toán FPS
    int   m_frameCount{0};
    float m_fpsAccumulator{0.0f};
    float m_currentFPS{0.0f};
};

} // namespace app
