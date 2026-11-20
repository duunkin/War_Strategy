#include "GameApp.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>

namespace app {

GameApp::GameApp() = default;

GameApp::~GameApp() {
    cleanup();
}

uint64_t GameApp::generateRandomSeed() {
    static uint64_t counter = 0;
    counter++;
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    // Trộn bit để tạo seed phân tán đều
    uint64_t z = static_cast<uint64_t>(now) + counter * 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void GameApp::regenerateMap(uint64_t seed) {
    m_currentSeed = seed;
    m_mapGenerator.generate(m_worldMap, m_currentSeed, m_mapConfig);
    m_worldSim.spawnPlayerDivisions(m_currentSeed);
    if (m_window.getSDLRenderer()) {
        m_mapRenderer.updateMinimapTexture(m_window.getSDLRenderer(), m_worldMap);
    }
}

SDL_Rect GameApp::getNewMapButtonRect(int screenWidth) const {
    constexpr int btnW = 180;
    constexpr int btnH = 38;
    constexpr int margin = 16;
    return SDL_Rect{
        .x = screenWidth - btnW - margin,
        .y = margin,
        .w = btnW,
        .h = btnH
    };
}

bool GameApp::init() {
    // 1. Khởi tạo hệ thống video của SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[GameApp] SDL_Init loi: " << SDL_GetError() << std::endl;
        return false;
    }

    // 2. Mở cửa sổ 1280x720 có ghi rõ thời gian build ở tiêu đề
    constexpr int initialWidth = 1280;
    constexpr int initialHeight = 720;
    const std::string windowTitle = "War Strategy - Milestone 3a (Build: " __DATE__ " " __TIME__ ")";
    if (!m_window.init(windowTitle, initialWidth, initialHeight)) {
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

    // 4. Nạp cấu hình sim và sinh bản đồ lần đầu với seed ban đầu
    if (!m_worldSim.loadConfig()) {
        std::cerr << "[GameApp] LOI: Khong the nap cau hinh game_config.json!" << std::endl;
        return false;
    }
    regenerateMap(m_currentSeed);

    // 5. Khởi tạo vị trí camera vào trung tâm bản đồ
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
#ifdef WS_DEBUG_INPUT
            const char* scancodeName = SDL_GetScancodeName(event.key.keysym.scancode);
            const char* keyName = SDL_GetKeyName(event.key.keysym.sym);
            SDL_Log("[KEY DOWN] scancode=%d (%s), sym=%d (%s), repeat=%d",
                    static_cast<int>(event.key.keysym.scancode),
                    scancodeName ? scancodeName : "Unknown",
                    static_cast<int>(event.key.keysym.sym),
                    keyName ? keyName : "Unknown",
                    static_cast<int>(event.key.repeat));
            std::fflush(stdout);
#endif

            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_running = false;
            } else if (event.key.repeat == 0 &&
                       (event.key.keysym.scancode == SDL_SCANCODE_R ||
                        event.key.keysym.scancode == SDL_SCANCODE_F5)) {
                // Phím R và F5: sinh bản đồ mới với seed ngẫu nhiên
                uint64_t oldSeed = m_currentSeed;
                uint64_t newSeed = generateRandomSeed();
                while (newSeed == oldSeed) {
                    newSeed = generateRandomSeed();
                }

                regenerateMap(newSeed);

                // Hiển thị thông báo trên màn hình trong 2 giây
                m_notificationText = "Seed moi: " + std::to_string(newSeed);
                m_notificationTimer = 2.0f;
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                // 1. Kiểm tra bấm vào nút "Tao ban do moi" ở góc trên bên phải
                SDL_Rect btnRect = getNewMapButtonRect(winW);
                if (event.button.x >= btnRect.x && event.button.x < btnRect.x + btnRect.w &&
                    event.button.y >= btnRect.y && event.button.y < btnRect.y + btnRect.h) {

                    uint64_t oldSeed = m_currentSeed;
                    uint64_t newSeed = generateRandomSeed();
                    while (newSeed == oldSeed) {
                        newSeed = generateRandomSeed();
                    }

                    regenerateMap(newSeed);

                    m_notificationText = "Seed moi: " + std::to_string(newSeed);
                    m_notificationTimer = 2.0f;
                }
                // 2. Kiểm tra xem người dùng có click vào Minimap không
                else if (m_mapRenderer.handleMinimapClick(event.button.x, event.button.y, winW, winH, m_worldMap, m_camera)) {
                    m_isDraggingMinimap = true;
                }
                // 3. Chuột trái trên chiến trường: Bắt đầu chọn quân hoặc kéo chọn vùng
                else {
                    m_boxStartX = event.button.x;
                    m_boxStartY = event.button.y;
                    m_boxCurrentX = event.button.x;
                    m_boxCurrentY = event.button.y;
                    m_isBoxSelecting = true;
                }
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                // Click chuột phải: ra lệnh di chuyển cho các chấm Player đang được chọn
                float worldX = m_camera.screenToWorldX(static_cast<float>(event.button.x));
                float worldY = m_camera.screenToWorldY(static_cast<float>(event.button.y));
                int targetTileX = static_cast<int>(worldX) / m_worldMap.getTileSize();
                int targetTileY = static_cast<int>(worldY) / m_worldMap.getTileSize();

                if (m_worldMap.isInBounds(targetTileX, targetTileY)) {
                    std::vector<uint32_t> selectedIds;
                    for (const auto& u : m_worldSim.getUnits()) {
                        if (u.isSelected() && u.getFaction() == sim::Faction::Player) {
                            selectedIds.push_back(u.getId());
                        }
                    }
                    if (!selectedIds.empty()) {
                        m_worldSim.queueMoveCommand(selectedIds, targetTileX, targetTileY);
                    }
                }
            }
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (m_isDraggingMinimap) {
                    m_isDraggingMinimap = false;
                } else if (m_isBoxSelecting) {
                    m_isBoxSelecting = false;
                    int dragDist = std::max(std::abs(m_boxCurrentX - m_boxStartX), std::abs(m_boxCurrentY - m_boxStartY));
                    bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
                    float pixelScale = static_cast<float>(m_worldSim.getConfig().getTilePixels());

                    // A. Click đơn lẻ (kéo thả <= 5 pixel)
                    if (dragDist <= 5) {
                        uint32_t clickedPlayerId = 0;
                        uint32_t clickedEnemyId = 0;
                        float bestDistSq = 999999.0f;

                        for (const auto& u : m_worldSim.getUnits()) {
                            if (u.isDead()) continue;
                            float wx = static_cast<float>(u.getPosX()) * pixelScale / 1000.0f;
                            float wy = static_cast<float>(u.getPosY()) * pixelScale / 1000.0f;
                            float sx = m_camera.worldToScreenX(wx);
                            float sy = m_camera.worldToScreenY(wy);
                            float dSq = (sx - m_boxStartX) * (sx - m_boxStartX) + (sy - m_boxStartY) * (sy - m_boxStartY);
                            float hitRadius = std::max(12.0f, 16.0f * m_camera.getZoom());
                            if (dSq <= hitRadius * hitRadius && dSq < bestDistSq) {
                                bestDistSq = dSq;
                                if (u.getFaction() == sim::Faction::Player) {
                                    clickedPlayerId = u.getId();
                                    clickedEnemyId = 0;
                                } else {
                                    clickedEnemyId = u.getId();
                                    clickedPlayerId = 0;
                                }
                            }
                        }

                        if (clickedPlayerId != 0) {
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(u.getId() == clickedPlayerId);
                                }
                            } else {
                                auto* u = m_worldSim.getUnitById(clickedPlayerId);
                                if (u) u->setSelected(!u->isSelected());
                            }
                            m_inspectedUnitId = clickedPlayerId;
                        } else if (clickedEnemyId != 0) {
                            // Chấm Enemy chỉ để xem, chưa có lệnh
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(false);
                                }
                            }
                            m_inspectedUnitId = clickedEnemyId;
                        } else {
                            // Click chuột trái vào chỗ trống: bỏ chọn
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(false);
                                }
                                m_inspectedUnitId = 0;
                            }
                        }
                    }
                    // B. Kéo thả hình chữ nhật chọn vùng
                    else {
                        int minX = std::min(m_boxStartX, m_boxCurrentX);
                        int maxX = std::max(m_boxStartX, m_boxCurrentX);
                        int minY = std::min(m_boxStartY, m_boxCurrentY);
                        int maxY = std::max(m_boxStartY, m_boxCurrentY);

                        uint32_t firstSelected = 0;
                        for (auto& u : m_worldSim.getUnits()) {
                            if (u.isDead() || u.getFaction() != sim::Faction::Player) continue;

                            float wx = static_cast<float>(u.getPosX()) * pixelScale / 1000.0f;
                            float wy = static_cast<float>(u.getPosY()) * pixelScale / 1000.0f;
                            float sx = m_camera.worldToScreenX(wx);
                            float sy = m_camera.worldToScreenY(wy);

                            if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY) {
                                u.setSelected(true);
                                if (firstSelected == 0) firstSelected = u.getId();
                            } else if (!shiftHeld) {
                                u.setSelected(false);
                            }
                        }
                        if (firstSelected != 0) {
                            m_inspectedUnitId = firstSelected;
                        }
                    }
                }
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            // Khi đang kéo chuột trên Minimap, liên tục cập nhật vị trí camera
            if (m_isDraggingMinimap) {
                m_mapRenderer.handleMinimapClick(event.motion.x, event.motion.y, winW, winH, m_worldMap, m_camera);
            } else if (m_isBoxSelecting) {
                m_boxCurrentX = event.motion.x;
                m_boxCurrentY = event.motion.y;
            }
        }

        // Cập nhật kích thước cửa sổ khi thay đổi
        m_window.handleEvent(event);

        // Sự kiện chuột (lăn chuột zoom camera)
        m_camera.handleEvent(event, winW, winH, worldW, worldH);
    }
}

void GameApp::update(float deltaTime) {
    int winW = m_window.getWidth();
    int winH = m_window.getHeight();
    int worldW = m_worldMap.getWorldWidth();
    int worldH = m_worldMap.getWorldHeight();

    // Giảm dần bộ đếm thời gian hiển thị thông báo
    if (m_notificationTimer > 0.0f) {
        m_notificationTimer -= deltaTime;
        if (m_notificationTimer < 0.0f) {
            m_notificationTimer = 0.0f;
        }
    }

    // Cập nhật di chuyển camera (phím WASD / mũi tên và rê chuột ra mép nếu không drag minimap)
    if (!m_isDraggingMinimap) {
        m_camera.update(deltaTime, winW, winH, worldW, worldH);
    }

    // Cập nhật mô phỏng sim với bước thời gian cố định 20 tick/giây qua accumulator
    m_simAccumulator += deltaTime;
    const int tps = m_worldSim.getConfig().getTickRate();
    const float tickInterval = (tps > 0) ? (1.0f / static_cast<float>(tps)) : 0.05f;
    while (m_simAccumulator >= tickInterval) {
        m_simAccumulator -= tickInterval;
        m_worldSim.tick();
    }

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

    // 2. Vẽ bản đồ thế giới tô màu theo địa hình và Minimap ở góc dưới phải
    m_mapRenderer.render(renderer, m_worldMap, m_camera, winW, winH);

    // 2b. Vẽ các đơn vị quân đội theo phe và đường đi (M4)
    m_unitRenderer.render(renderer, m_worldSim.getUnits(), m_camera, winW, winH, m_worldSim.getConfig().getTilePixels());

    // 2c. Vẽ khung chữ nhật kéo thả chọn quân nếu đang drag
    if (m_isBoxSelecting) {
        m_unitRenderer.renderSelectionBox(renderer, m_boxStartX, m_boxStartY, m_boxCurrentX, m_boxCurrentY);
    }

    // 2d. Vẽ bảng thông tin nhỏ nếu có đơn vị đang chọn hoặc đang xem (M4)
    const sim::Unit* inspected = nullptr;
    if (m_inspectedUnitId != 0) {
        inspected = m_worldSim.getUnitById(m_inspectedUnitId);
    }
    if (!inspected || inspected->isDead()) {
        for (const auto& u : m_worldSim.getUnits()) {
            if (u.isSelected()) {
                inspected = &u;
                break;
            }
        }
    }
    if (inspected && !inspected->isDead()) {
        m_unitRenderer.renderInfoPanel(renderer, *inspected, m_textRenderer, winW, winH);
    }

    // 3. Hiển thị FPS và thông tin Seed ở góc trên bên trái
    char infoBuffer[128];
    std::snprintf(infoBuffer, sizeof(infoBuffer), "FPS: %.0f", m_currentFPS);
    SDL_Color textColor{240, 240, 240, 255};
    m_textRenderer.renderText(renderer, infoBuffer, 16, 16, textColor, true);

    char seedBuffer[128];
    std::snprintf(seedBuffer, sizeof(seedBuffer), "Seed: %llu  |  [R / F5] Tao ban do moi", static_cast<unsigned long long>(m_currentSeed));
    SDL_Color seedColor{255, 220, 100, 255};
    m_textRenderer.renderText(renderer, seedBuffer, 16, 42, seedColor, true);

    // 4. Hiển thị thông báo "Seed moi: <seed>" nếu đang trong thời gian 2 giây
    if (m_notificationTimer > 0.0f) {
        SDL_Rect notifBg{16, 70, 360, 32};
        SDL_SetRenderDrawColor(renderer, 20, 60, 30, 220);
        SDL_RenderFillRect(renderer, &notifBg);
        SDL_SetRenderDrawColor(renderer, 60, 220, 100, 255);
        SDL_RenderDrawRect(renderer, &notifBg);

        SDL_Color notifTextColor{80, 255, 140, 255};
        m_textRenderer.renderText(renderer, m_notificationText, 24, 76, notifTextColor, false);
    }

    // 5. Vẽ nút bấm "Tao ban do moi" ở góc trên bên phải
    SDL_Rect btnRect = getNewMapButtonRect(winW);
    SDL_SetRenderDrawColor(renderer, 32, 44, 62, 230);
    SDL_RenderFillRect(renderer, &btnRect);
    SDL_SetRenderDrawColor(renderer, 220, 190, 90, 255);
    SDL_RenderDrawRect(renderer, &btnRect);

    SDL_Color btnTextColor{245, 245, 245, 255};
    m_textRenderer.renderText(renderer, "Tao ban do moi", btnRect.x + 14, btnRect.y + 9, btnTextColor, false);

    // 6. Hiển thị khung hình lên cửa sổ
    m_window.present();
}

void GameApp::cleanup() {
    m_mapRenderer.cleanup();
    m_textRenderer.cleanup();
    m_window.cleanup();
    SDL_Quit();
}

} // namespace app
