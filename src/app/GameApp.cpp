#include "GameApp.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>

namespace app {

GameApp::GameApp() = default;

GameApp::~GameApp() {
    cleanup();
}

bool GameApp::init() {
    // 1. Khởi tạo hệ thống video của SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[GameApp] SDL_Init loi: " << SDL_GetError() << std::endl;
        return false;
    }

    // 2. Mở cửa sổ 1280x720 có thể thay đổi kích thước
    constexpr int initialWidth = 1280;
    constexpr int initialHeight = 720;
    if (!m_window.init("War Strategy - Milestone 1", initialWidth, initialHeight)) {
        std::cerr << "[GameApp] Khoi tao cua so that bai!" << std::endl;
        return false;
    }

    // 3. Khởi tạo TextRenderer và nạp font
    std::vector<std::string> fontCandidates = {
        "assets/fonts/font.ttf",
        "font.ttf",
        "../assets/fonts/font.ttf",
        "../../assets/fonts/font.ttf"
    };
    m_textRenderer.init(fontCandidates, 18);

    // 4. Khởi tạo vị trí camera vào trung tâm bản đồ
    int worldW = m_worldMap.getWorldWidth();
    int worldH = m_worldMap.getWorldHeight();
    float startX = (static_cast<float>(worldW) - initialWidth) * 0.5f;
    float startY = (static_cast<float>(worldH) - initialHeight) * 0.5f;
    m_camera.setPosition(startX, startY);
    m_camera.clampToBounds(initialWidth, initialHeight, worldW, worldH);

    m_running = true;
    return true;
}

int GameApp::run() {
    if (!init()) {
        cleanup();
        return 1;
    }

    // Đo thời gian bằng SDL_GetPerformanceCounter để có độ chính xác cao
    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    // Vòng lặp game chính (Game Loop)
    while (m_running) {
        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(currentCounter - lastCounter) / static_cast<float>(frequency);
        lastCounter = currentCounter;

        // Giới hạn deltaTime tránh hiện tượng nhảy khung hình đột ngột khi kéo cửa sổ
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        processEvents();
        update(deltaTime);
        render();
    }

    cleanup();
    return 0;
}

void GameApp::processEvents() {
    SDL_Event event;
    int winW = m_window.getWidth();
    int winH = m_window.getHeight();
    int worldW = m_worldMap.getWorldWidth();
    int worldH = m_worldMap.getWorldHeight();

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_running = false;
            }
        }

        // Cập nhật kích thước cửa sổ khi thay đổi
        m_window.handleEvent(event);

        // Sự kiện chuột (lăn chuột zoom)
        m_camera.handleEvent(event, winW, winH, worldW, worldH);
    }
}

void GameApp::update(float deltaTime) {
    int winW = m_window.getWidth();
    int winH = m_window.getHeight();
    int worldW = m_worldMap.getWorldWidth();
    int worldH = m_worldMap.getWorldHeight();

    // Cập nhật di chuyển camera (phím WASD / mũi tên và rê chuột ra mép)
    m_camera.update(deltaTime, winW, winH, worldW, worldH);

    // Tính toán FPS (cập nhật số liệu mỗi 0.25 giây để hiển thị mượt mà)
    m_frameCount++;
    m_fpsAccumulator += deltaTime;
    if (m_fpsAccumulator >= 0.25f) {
        m_currentFPS = static_cast<float>(m_frameCount) / m_fpsAccumulator;
        m_frameCount = 0;
        m_fpsAccumulator = 0.0f;
    }
}

void GameApp::render() {
    int winW = m_window.getWidth();
    int winH = m_window.getHeight();
    SDL_Renderer* renderer = m_window.getSDLRenderer();

    // 1. Xóa màn hình với màu xám đen đậm cho khu vực ngoài biên bản đồ
    m_window.clear(14, 16, 20, 255);

    // 2. Vẽ bản đồ và hệ thống lưới ô 32x32 cho bản đồ 128x96
    m_mapRenderer.render(renderer, m_worldMap, m_camera, winW, winH);

    // 3. Hiển thị FPS và thông tin điều khiển ở góc trên màn hình
    char fpsBuffer[64];
    std::snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS: %.0f", m_currentFPS);
    SDL_Color textColor{240, 240, 240, 255};
    m_textRenderer.renderText(renderer, fpsBuffer, 16, 16, textColor, true);

    // 4. Hiển thị khung hình lên cửa sổ
    m_window.present();
}

void GameApp::cleanup() {
    m_textRenderer.cleanup();
    m_window.cleanup();
    SDL_Quit();
}

} // namespace app
