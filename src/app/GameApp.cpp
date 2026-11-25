#include "GameApp.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>

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
    m_enemyAI.reset(m_currentSeed);

    // Căn camera ban đầu tập trung vào khu vực spawn trung điểm cạnh trái của Player
    int tileSize = m_worldMap.getTileSize();
    float playerSpawnPixelX = static_cast<float>(m_worldSim.getPlayerSpawnAnchorX() * tileSize);
    float playerSpawnPixelY = static_cast<float>(m_worldSim.getPlayerSpawnAnchorY() * tileSize);
    float camStartX = playerSpawnPixelX - static_cast<float>(m_window.getWidth()) * 0.5f;
    float camStartY = playerSpawnPixelY - static_cast<float>(m_window.getHeight()) * 0.5f;
    m_camera.setPosition(camStartX, camStartY);
    m_camera.clampToBounds(m_window.getWidth(), m_window.getHeight(), m_worldMap.getWorldWidth(), m_worldMap.getWorldHeight());

    if (m_window.getSDLRenderer()) {
        sim::Faction viewingFaction = m_worldSim.getControlledFaction();
        m_mapRenderer.updateFogTexture(m_window.getSDLRenderer(), m_worldSim.getFogMap(viewingFaction), m_worldMap.getWidth(), m_worldMap.getHeight(), m_debugFogDisabled);
        m_mapRenderer.updateMinimapTexture(m_window.getSDLRenderer(), m_worldMap, m_worldSim.getFogMap(viewingFaction), m_worldSim.getUnits(), viewingFaction, m_debugFogDisabled);
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
    const std::string windowTitle = "War Strategy - Milestone 7 (Build: " __DATE__ " " __TIME__ ")";
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

    // 4. Nạp cấu hình sim và sinh bản đồ lần đầu với seed ban đầu (regenerateMap tự căn camera vào quân Player)
    if (!m_worldSim.loadConfig()) {
        std::cerr << "[GameApp] LOI: Khong the nap cau hinh game_config.json!" << std::endl;
        return false;
    }

    // M8: Nếu có nạp file Replay, tải file và đồng bộ seed
    if (!m_replayPath.empty()) {
        if (m_replayPlayer.loadFromFile(m_replayPath)) {
            m_isReplayMode = true;
            m_aiEnabled = false; // Tắt AI live khi xem replay
            m_aiSourceLabel = "REPLAY";
            m_currentSeed = m_replayPlayer.getSeed();
            std::cout << "[GameApp] REPLAY MODE: Nap thanh cong " << m_replayPath
                      << " | Seed: " << m_currentSeed
                      << " | Records: " << m_replayPlayer.getRecordCount()
                      << " | FinalTick: " << m_replayPlayer.getFinalTick() << std::endl;
            m_notificationText = "[REPLAY MODE] Da nap replay thanh cong!";
            m_notificationTimer = 3.0f;
        } else {
            std::cerr << "[GameApp] LOI: Khong the mo file replay: " << m_replayPath << std::endl;
        }
    }

    // M9: Nạp bộ não AI đã huấn luyện nếu không ở chế độ Replay
    if (!m_isReplayMode) {
        if (!m_aiParamsPath.empty()) {
            ai::AiParams loadedParams;
            ai::AiTrainingMetadata loadedMeta;
            if (ai::loadTrainedAiProfile(m_aiParamsPath, loadedParams, &loadedMeta)) {
                m_enemyAI.setParams(loadedParams);
                std::filesystem::path p(m_aiParamsPath);
                m_aiSourceLabel = "AI: Custom (" + p.filename().string() + ")";
                std::cout << "[GameApp] Da nap AiParams tu: " << m_aiParamsPath << std::endl;
            } else {
                std::cerr << "[GameApp] Khong the nap AiParams tu " << m_aiParamsPath << ", fallback mac dinh.\n";
                m_aiSourceLabel = "AI: Default";
            }
        } else if (std::filesystem::exists("data/ai_best_params.json")) {
            ai::AiParams loadedParams;
            ai::AiTrainingMetadata loadedMeta;
            if (ai::loadTrainedAiProfile("data/ai_best_params.json", loadedParams, &loadedMeta)) {
                m_enemyAI.setParams(loadedParams);
                if (loadedMeta.generation > 0) {
                    m_aiSourceLabel = "AI: Trained (Gen " + std::to_string(loadedMeta.generation) + ")";
                } else {
                    m_aiSourceLabel = "AI: Trained";
                }
                std::cout << "[GameApp] Phat hien va nap thanh cong data/ai_best_params.json (" << m_aiSourceLabel << ")" << std::endl;
            } else {
                m_aiSourceLabel = "AI: Default";
            }
        } else {
            m_aiSourceLabel = "AI: Default";
        }
    }

    regenerateMap(m_currentSeed);

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
#ifndef NDEBUG
            } else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F1) {
                // [DEBUG ONLY - F1] Chế độ điều khiển thử nghiệm đổi phe
                m_worldSim.toggleControlledFaction();
                // Bỏ chọn toàn bộ quân khi đổi phe (M6)
                for (auto& u : m_worldSim.getUnits()) {
                    u.setSelected(false);
                }
                m_inspectedUnitId = 0;
                bool isPlayer = (m_worldSim.getControlledFaction() == sim::Faction::Player);
                m_notificationText = isPlayer ? "[DEBUG F1] Dieu khien & Tam nhin: PHE XANH (Player)"
                                              : "[DEBUG F1] Dieu khien & Tam nhin: PHE DO (Enemy)";
                m_notificationTimer = 2.0f;
                if (m_window.getSDLRenderer()) {
                    sim::Faction vf = m_worldSim.getControlledFaction();
                    m_mapRenderer.updateFogTexture(m_window.getSDLRenderer(), m_worldSim.getFogMap(vf), m_worldMap.getWidth(), m_worldMap.getHeight(), m_debugFogDisabled);
                    m_mapRenderer.updateMinimapTexture(m_window.getSDLRenderer(), m_worldMap, m_worldSim.getFogMap(vf), m_worldSim.getUnits(), vf, m_debugFogDisabled);
                }
            } else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F2) {
                // [DEBUG ONLY - F2] Bật/tắt sương mù chiến tranh để debug
                m_debugFogDisabled = !m_debugFogDisabled;
                m_notificationText = m_debugFogDisabled ? "[DEBUG F2] Suong mu: TAT (Hien toan ban do)"
                                                        : "[DEBUG F2] Suong mu: BAT (Theo tam nhin thuc te)";
                m_notificationTimer = 2.0f;
                if (m_window.getSDLRenderer()) {
                    sim::Faction vf = m_worldSim.getControlledFaction();
                    m_mapRenderer.updateFogTexture(m_window.getSDLRenderer(), m_worldSim.getFogMap(vf), m_worldMap.getWidth(), m_worldMap.getHeight(), m_debugFogDisabled);
                    m_mapRenderer.updateMinimapTexture(m_window.getSDLRenderer(), m_worldMap, m_worldSim.getFogMap(vf), m_worldSim.getUnits(), vf, m_debugFogDisabled);
                }
#endif
            } else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_F3) {
                // [DEBUG F3] Bật/tắt AI phe Enemy để kiểm thử
                m_aiEnabled = !m_aiEnabled;
                m_notificationText = m_aiEnabled ? "[DEBUG F3] AI Phe Do (Enemy): BAT"
                                                 : "[DEBUG F3] AI Phe Do (Enemy): TAT (Dung yen)";
                m_notificationTimer = 2.0f;
            } else if (event.key.repeat == 0 && event.key.keysym.sym == SDLK_SPACE && m_isReplayMode) {
                m_replayPaused = !m_replayPaused;
                m_notificationText = m_replayPaused ? "[REPLAY] TAM DUNG (PAUSED)" : "[REPLAY] TIEP TUC (PLAYING)";
                m_notificationTimer = 1.5f;
            } else if (event.key.repeat == 0 && m_isReplayMode &&
                       (event.key.keysym.sym == SDLK_LEFTBRACKET || event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS)) {
                if (m_replaySpeed >= 8.0f) m_replaySpeed = 4.0f;
                else if (m_replaySpeed >= 4.0f) m_replaySpeed = 2.0f;
                else if (m_replaySpeed >= 2.0f) m_replaySpeed = 1.0f;
                else if (m_replaySpeed >= 1.0f) m_replaySpeed = 0.5f;
                m_notificationText = "[REPLAY] Toc do: " + std::to_string(m_replaySpeed).substr(0, 3) + "x";
                m_notificationTimer = 1.5f;
            } else if (event.key.repeat == 0 && m_isReplayMode &&
                       (event.key.keysym.sym == SDLK_RIGHTBRACKET || event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_EQUALS || event.key.keysym.sym == SDLK_KP_PLUS)) {
                if (m_replaySpeed <= 0.5f) m_replaySpeed = 1.0f;
                else if (m_replaySpeed <= 1.0f) m_replaySpeed = 2.0f;
                else if (m_replaySpeed <= 2.0f) m_replaySpeed = 4.0f;
                else if (m_replaySpeed <= 4.0f) m_replaySpeed = 8.0f;
                m_notificationText = "[REPLAY] Toc do: " + std::to_string(m_replaySpeed).substr(0, 3) + "x";
                m_notificationTimer = 1.5f;
            } else if (event.key.repeat == 0 &&
                       (event.key.keysym.scancode == SDL_SCANCODE_R ||
                        event.key.keysym.scancode == SDL_SCANCODE_F5)) {
                if (m_isReplayMode) {
                    m_notificationText = "[REPLAY MODE] Khong the doi ban do khi dang phat replay!";
                    m_notificationTimer = 2.0f;
                    continue;
                }
                // Phím R và F5: sinh bản đồ mới với seed ngẫu nhiên
                uint64_t oldSeed = m_currentSeed;
                uint64_t newSeed = generateRandomSeed();
                while (newSeed == oldSeed) {
                    newSeed = generateRandomSeed();
                }

                regenerateMap(newSeed);

                m_notificationText = "Seed moi: " + std::to_string(newSeed);
                m_notificationTimer = 2.0f;
            } else {
                m_camera.handleEvent(event, winW, winH, worldW, worldH);
            }
        } else if (event.type == SDL_KEYUP) {
            m_camera.handleEvent(event, winW, winH, worldW, worldH);
        } else if (event.type == SDL_MOUSEWHEEL) {
            m_camera.handleEvent(event, winW, winH, worldW, worldH);
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                SDL_Rect btnRect = getNewMapButtonRect(winW);
                // 1. Kiểm tra xem người dùng có click vào nút "Tao ban do moi" không
                if (event.button.x >= btnRect.x && event.button.x <= btnRect.x + btnRect.w &&
                    event.button.y >= btnRect.y && event.button.y <= btnRect.y + btnRect.h) {
                    if (m_isReplayMode) {
                        m_notificationText = "[REPLAY MODE] Khong the doi ban do khi dang phat replay!";
                        m_notificationTimer = 2.0f;
                        continue;
                    }
                    uint64_t oldSeed = m_currentSeed;
                    uint64_t newSeed = generateRandomSeed();
                    while (newSeed == oldSeed) {
                        newSeed = generateRandomSeed();
                    }

                    regenerateMap(newSeed);

                    m_notificationText = "Seed moi: " + std::to_string(newSeed);
                    m_notificationTimer = 2.0f;
                }
                // 2. Kiểm tra xem người dùng có click vào Minimap không (M6: chỉ dịch camera, không chọn quân)
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
                // Trong Replay Mode, khóa nhận lệnh điều khiển quân của người chơi
                if (m_isReplayMode) {
                    continue;
                }
                // Click chuột phải:
                // M6: Chuột phải trên minimap ra lệnh di chuyển cho các chấm đang chọn tới điểm tương ứng
                int targetTileX = 0;
                int targetTileY = 0;
                bool fromMinimap = m_mapRenderer.handleMinimapRightClick(event.button.x, event.button.y, winW, winH, m_worldMap, targetTileX, targetTileY);
                if (!fromMinimap) {
                    float worldX = m_camera.screenToWorldX(static_cast<float>(event.button.x));
                    float worldY = m_camera.screenToWorldY(static_cast<float>(event.button.y));
                    targetTileX = static_cast<int>(worldX) / m_worldMap.getTileSize();
                    targetTileY = static_cast<int>(worldY) / m_worldMap.getTileSize();
                }

                if (m_worldMap.isInBounds(targetTileX, targetTileY)) {
                    sim::Faction ctrlFaction = m_worldSim.getControlledFaction();
                    std::vector<uint32_t> selectedIds;
                    for (const auto& u : m_worldSim.getUnits()) {
                        if (u.isSelected() && u.getFaction() == ctrlFaction) {
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
                    sim::Faction ctrlFaction = m_worldSim.getControlledFaction();

                    // A. Click đơn lẻ (kéo thả <= 5 pixel)
                    if (dragDist <= 5) {
                        uint32_t clickedControlledId = 0;
                        uint32_t clickedOtherId = 0;
                        float bestDistSq = 999999.0f;

                        for (const auto& u : m_worldSim.getUnits()) {
                            if (u.isDead()) continue;

                            // M6: Địch trong sương mù không thể click chọn hoặc xem
                            if (u.getFaction() != ctrlFaction && !m_debugFogDisabled && !m_worldSim.isUnitVisible(u, ctrlFaction)) {
                                continue;
                            }

                            float wx = static_cast<float>(u.getPosX()) * pixelScale / 1000.0f;
                            float wy = static_cast<float>(u.getPosY()) * pixelScale / 1000.0f;
                            float sx = m_camera.worldToScreenX(wx);
                            float sy = m_camera.worldToScreenY(wy);
                            float dSq = (sx - m_boxStartX) * (sx - m_boxStartX) + (sy - m_boxStartY) * (sy - m_boxStartY);
                            float hitRadius = std::max(12.0f, 16.0f * m_camera.getZoom());
                            if (dSq <= hitRadius * hitRadius && dSq < bestDistSq) {
                                bestDistSq = dSq;
                                if (u.getFaction() == ctrlFaction) {
                                    clickedControlledId = u.getId();
                                    clickedOtherId = 0;
                                } else {
                                    clickedOtherId = u.getId();
                                    clickedControlledId = 0;
                                }
                            }
                        }

                        if (clickedControlledId != 0) {
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(u.getId() == clickedControlledId);
                                }
                            } else {
                                auto* u = m_worldSim.getUnitById(clickedControlledId);
                                if (u) u->setSelected(!u->isSelected());
                            }
                            m_inspectedUnitId = clickedControlledId;
                        } else if (clickedOtherId != 0) {
                            // Chấm phe đối diện chỉ để xem, không nhận lệnh
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(false);
                                }
                            }
                            m_inspectedUnitId = clickedOtherId;
                        } else {
                            // Click chuột trái vào chỗ trống: bỏ chọn
                            if (!shiftHeld) {
                                for (auto& u : m_worldSim.getUnits()) {
                                    u.setSelected(false);
                                }
                            }
                            m_inspectedUnitId = 0;
                        }
                    }
                    // B. Kéo thả hình chữ nhật (Box Selection)
                    else {
                        int boxMinX = std::min(m_boxStartX, m_boxCurrentX);
                        int boxMaxX = std::max(m_boxStartX, m_boxCurrentX);
                        int boxMinY = std::min(m_boxStartY, m_boxCurrentY);
                        int boxMaxY = std::max(m_boxStartY, m_boxCurrentY);

                        if (!shiftHeld) {
                            for (auto& u : m_worldSim.getUnits()) {
                                u.setSelected(false);
                            }
                        }

                        uint32_t lastSelectedId = 0;
                        for (auto& u : m_worldSim.getUnits()) {
                            if (u.isDead() || u.getFaction() != ctrlFaction) continue;

                            float wx = static_cast<float>(u.getPosX()) * pixelScale / 1000.0f;
                            float wy = static_cast<float>(u.getPosY()) * pixelScale / 1000.0f;
                            float sx = m_camera.worldToScreenX(wx);
                            float sy = m_camera.worldToScreenY(wy);

                            if (sx >= boxMinX && sx <= boxMaxX && sy >= boxMinY && sy <= boxMaxY) {
                                u.setSelected(true);
                                lastSelectedId = u.getId();
                            }
                        }
                        if (lastSelectedId != 0) {
                            m_inspectedUnitId = lastSelectedId;
                        }
                    }
                }
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            if (m_isDraggingMinimap) {
                m_mapRenderer.handleMinimapClick(event.motion.x, event.motion.y, winW, winH, m_worldMap, m_camera);
            } else if (m_isBoxSelecting) {
                m_boxCurrentX = event.motion.x;
                m_boxCurrentY = event.motion.y;
            }
        }
    }
}

void GameApp::update(float deltaTime) {
    int winW = m_window.getWidth();
    int winH = m_window.getHeight();
    int worldW = m_worldMap.getWorldWidth();
    int worldH = m_worldMap.getWorldHeight();

    // Cập nhật bộ đếm thông báo
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
    if (!m_isReplayMode || !m_replayPaused) {
        float effectiveDt = m_isReplayMode ? (deltaTime * m_replaySpeed) : deltaTime;
        m_simAccumulator += effectiveDt;
    }
    const int tps = m_worldSim.getConfig().getTickRate();
    const float tickInterval = (tps > 0) ? (1.0f / static_cast<float>(tps)) : 0.05f;
    bool simTicked = false;
    while (m_simAccumulator >= tickInterval) {
        m_simAccumulator -= tickInterval;
        if (m_isReplayMode) {
            m_replayPlayer.applyCommandsForTick(m_worldSim.getCurrentTick(), m_worldSim);
        } else if (m_aiEnabled) {
            m_enemyAI.update(m_worldSim);
        }
        m_worldSim.tick();
        simTicked = true;
    }

    // M6: Texture minimap và sương mù chỉ cập nhật theo tick sim để tối ưu hiệu năng
    if (simTicked && m_window.getSDLRenderer()) {
        sim::Faction viewingFaction = m_worldSim.getControlledFaction();
        m_mapRenderer.updateFogTexture(m_window.getSDLRenderer(), m_worldSim.getFogMap(viewingFaction), m_worldMap.getWidth(), m_worldMap.getHeight(), m_debugFogDisabled);
        m_mapRenderer.updateMinimapTexture(m_window.getSDLRenderer(), m_worldMap, m_worldSim.getFogMap(viewingFaction), m_worldSim.getUnits(), viewingFaction, m_debugFogDisabled);
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
    sim::Faction viewingFaction = m_worldSim.getControlledFaction();

    // 1. Xóa màn hình với màu xám đen đậm cho khu vực ngoài biên bản đồ
    m_window.clear(14, 16, 20, 255);

    // 2. Vẽ bản đồ thế giới tô màu theo địa hình
    m_mapRenderer.renderWorldTerrain(renderer, m_worldMap, m_camera, winW, winH);

    // 2a. Vẽ lớp phủ sương mù chiến tranh 128x96 phóng to có lọc tuyến tính (M6)
    m_mapRenderer.renderFog(renderer, m_camera, winW, winH, m_worldMap.getWorldWidth(), m_worldMap.getWorldHeight(), m_debugFogDisabled);

    // 2b. Vẽ các đơn vị quân đội theo phe và đường đi (M6: lọc theo sương mù)
    auto isUnitVis = [this, viewingFaction](const sim::Unit& u) {
        return m_worldSim.isUnitVisible(u, viewingFaction);
    };
    m_unitRenderer.render(renderer, m_worldSim.getUnits(), m_camera, winW, winH, isUnitVis, viewingFaction, m_debugFogDisabled, m_worldSim.getConfig().getTilePixels());

    // 2b2. Vẽ các đường đạn đang hoạt động trên chiến trường (M6: lọc theo sương mù)
    auto isTileVis = [this, viewingFaction](int tx, int ty) {
        return m_worldSim.isTileVisible(tx, ty, viewingFaction);
    };
    m_unitRenderer.renderTracers(renderer, m_worldSim.getActiveTracers(), m_camera, winW, winH, isTileVis, m_debugFogDisabled, m_worldSim.getConfig().getTilePixels());

    // 2c. Vẽ khung chữ nhật kéo thả chọn quân nếu đang drag
    if (m_isBoxSelecting) {
        m_unitRenderer.renderSelectionBox(renderer, m_boxStartX, m_boxStartY, m_boxCurrentX, m_boxCurrentY);
    }

    // 2c2. Hiển thị MINIMAP ở góc dưới phải màn hình (M6: chỉ cập nhật theo tick sim)
    m_mapRenderer.renderMinimap(renderer, m_worldMap, m_camera, winW, winH);

    // 2d. Vẽ bảng thông tin nhỏ nếu có đơn vị đang chọn hoặc đang xem (M4/M6: chỉ hiện nếu đang nhìn thấy)
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
        bool canSee = (inspected->getFaction() == viewingFaction) || m_debugFogDisabled || m_worldSim.isUnitVisible(*inspected, viewingFaction);
        if (canSee) {
            m_unitRenderer.renderInfoPanel(renderer, *inspected, m_textRenderer, winW, winH);
        }
    }

    // 3. Hiển thị FPS và thông tin thời gian cập nhật tầm nhìn Vision ở góc trên bên trái
    char infoBuffer[128];
    std::snprintf(infoBuffer, sizeof(infoBuffer), "FPS: %.0f  |  Vision: %.3f ms", m_currentFPS, m_worldSim.getLastVisionUpdateTimeMs());
    SDL_Color textColor{240, 240, 240, 255};
    m_textRenderer.renderText(renderer, infoBuffer, 16, 16, textColor, true);

    char seedBuffer[128];
    std::snprintf(seedBuffer, sizeof(seedBuffer), "Seed: %llu  |  [R / F5] Tao ban do moi", static_cast<unsigned long long>(m_currentSeed));
    SDL_Color seedColor{255, 220, 100, 255};
    m_textRenderer.renderText(renderer, seedBuffer, 16, 42, seedColor, true);

#ifndef NDEBUG
    // [DEBUG ONLY - F1 & F2 & F3]
    bool isPlayerCtrl = (viewingFaction == sim::Faction::Player);
    char f1Buffer[160];
    std::snprintf(f1Buffer, sizeof(f1Buffer), "[F1] Phe: %s  |  [F2] Suong mu: %s  |  [F3] AI: %s (%s)",
                  isPlayerCtrl ? "PHE XANH (Player)" : "PHE DO (Enemy)",
                  m_debugFogDisabled ? "TAT (Debug)" : "BAT",
                  m_aiEnabled ? "BAT" : "TAT",
                  m_aiSourceLabel.c_str());
    SDL_Color f1Color = isPlayerCtrl ? SDL_Color{100, 200, 255, 255} : SDL_Color{255, 120, 110, 255};
    m_textRenderer.renderText(renderer, f1Buffer, 16, 68, f1Color, true);
#endif

    // 4. Hiển thị đồng hồ thời gian còn lại của trận đấu ở giữa phía trên màn hình (M5)
    int remainingSec = m_worldSim.getRemainingTimeSec();
    int mm = remainingSec / 60;
    int ss = remainingSec % 60;
    char timerBuffer[64];
    std::snprintf(timerBuffer, sizeof(timerBuffer), "Con lai: %02d:%02d", mm, ss);
    int timerW = 160;
    int timerH = 32;
    int timerX = (winW - timerW) / 2;
    int timerY = 16;
    SDL_Rect timerBg{timerX, timerY, timerW, timerH};
    SDL_SetRenderDrawColor(renderer, 20, 26, 36, 230);
    SDL_RenderFillRect(renderer, &timerBg);
    SDL_SetRenderDrawColor(renderer, 70, 160, 240, 255);
    SDL_RenderDrawRect(renderer, &timerBg);
    SDL_Color timerColor{235, 245, 255, 255};
    m_textRenderer.renderText(renderer, timerBuffer, timerX + 16, timerY + 7, timerColor, false);

    // 4b. Hiển thị banner Replay Mode nổi bật (M8)
    if (m_isReplayMode) {
        char repBuf[160];
        std::snprintf(repBuf, sizeof(repBuf), "[REPLAY MODE] %s (%.1fx) | Tick: %llu/%llu | [Space: Pause, [-][+]: Speed, F2: FOW]",
                      m_replayPaused ? "PAUSED" : "PLAYING",
                      m_replaySpeed,
                      static_cast<unsigned long long>(m_worldSim.getCurrentTick()),
                      static_cast<unsigned long long>(m_replayPlayer.getFinalTick()));
        int rW = 680;
        int rH = 32;
        int rX = (winW - rW) / 2;
        int rY = 56;
        SDL_Rect rBg{rX, rY, rW, rH};
        SDL_SetRenderDrawColor(renderer, 25, 25, 40, 235);
        SDL_RenderFillRect(renderer, &rBg);
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, 255);
        SDL_RenderDrawRect(renderer, &rBg);
        SDL_Color repColor{255, 225, 90, 255};
        m_textRenderer.renderText(renderer, repBuf, rX + 14, rY + 7, repColor, false);
    }

    // 5. Hiển thị thông báo tạm nếu đang trong thời gian timer
    if (m_notificationTimer > 0.0f) {
        SDL_Rect notifBg{16, 96, 420, 32};
        SDL_SetRenderDrawColor(renderer, 20, 60, 30, 220);
        SDL_RenderFillRect(renderer, &notifBg);
        SDL_SetRenderDrawColor(renderer, 60, 220, 100, 255);
        SDL_RenderDrawRect(renderer, &notifBg);

        SDL_Color notifTextColor{80, 255, 140, 255};
        m_textRenderer.renderText(renderer, m_notificationText, 24, 102, notifTextColor, false);
    }

    // 6. Vẽ nút bấm "Tao ban do moi" ở góc trên bên phải
    SDL_Rect btnRect = getNewMapButtonRect(winW);
    SDL_SetRenderDrawColor(renderer, 32, 44, 62, 230);
    SDL_RenderFillRect(renderer, &btnRect);
    SDL_SetRenderDrawColor(renderer, 220, 190, 90, 255);
    SDL_RenderDrawRect(renderer, &btnRect);

    SDL_Color btnTextColor{245, 245, 245, 255};
    m_textRenderer.renderText(renderer, "Tao ban do moi", btnRect.x + 14, btnRect.y + 9, btnTextColor, false);

    // 7. Bảng kết quả trận đấu giữa màn hình khi ván kết thúc (M5)
    if (m_worldSim.isGameOver()) {
        const auto& endInfo = m_worldSim.getMatchEndInfo();
        int dialogW = 560;
        int dialogH = 170;
        int dialogX = (winW - dialogW) / 2;
        int dialogY = (winH - dialogH) / 2;

        SDL_Rect modalBg{dialogX, dialogY, dialogW, dialogH};
        SDL_SetRenderDrawColor(renderer, 15, 18, 26, 245);
        SDL_RenderFillRect(renderer, &modalBg);

        SDL_Color headerColor{255, 255, 255, 255};
        std::string headerText;
        if (endInfo.result == sim::MatchResult::Victory) {
            headerText = "=== CHIEN THANG ===";
            headerColor = SDL_Color{80, 240, 120, 255};
            SDL_SetRenderDrawColor(renderer, 60, 220, 100, 255);
        } else if (endInfo.result == sim::MatchResult::Defeat) {
            headerText = "=== THAT BAI ===";
            headerColor = SDL_Color{255, 75, 75, 255};
            SDL_SetRenderDrawColor(renderer, 240, 50, 50, 255);
        } else {
            headerText = "=== HOA TRAN ===";
            headerColor = SDL_Color{255, 220, 80, 255};
            SDL_SetRenderDrawColor(renderer, 250, 200, 60, 255);
        }
        SDL_RenderDrawRect(renderer, &modalBg);

        m_textRenderer.renderText(renderer, headerText, dialogX + 175, dialogY + 20, headerColor, false);
        SDL_Color reasonColor{240, 240, 240, 255};
        m_textRenderer.renderText(renderer, endInfo.reason, dialogX + 24, dialogY + 70, reasonColor, false);

        std::string restartHint = "[R / F5] Tao tran dau moi  |  [ESC] Thoat";
        SDL_Color hintColor{180, 190, 205, 255};
        m_textRenderer.renderText(renderer, restartHint, dialogX + 120, dialogY + 120, hintColor, false);
    }

    // 8. Hiển thị khung hình lên cửa sổ
    m_window.present();
}

void GameApp::cleanup() {
    m_mapRenderer.cleanup();
    m_textRenderer.cleanup();
    m_window.cleanup();
    SDL_Quit();
}

} // namespace app
