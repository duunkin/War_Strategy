#pragma once

#include "../render/Window.hpp"
#include "../render/Camera.hpp"
#include "../render/MapRenderer.hpp"
#include "../render/TextRenderer.hpp"
#include "../render/UnitRenderer.hpp"
#include "../sim/map/WorldMap.hpp"
#include "../sim/WorldSim.hpp"
#include "../mapgen/MapGenerator.hpp"
#include "../mapgen/MapGenConfig.hpp"
#include "../ai/TacticalAI.hpp"
#include "../ai/AiParams.hpp"
#include "../ai/AiTelemetry.hpp"
#include "../replay/Replay.hpp"

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

    // Cấu hình Replay (M8)
    void setReplayFile(const std::string& path) { m_replayPath = path; }

    // Cấu hình AI Params nạp từ file bên ngoài (M9)
    void setAiParamsFile(const std::string& path) { m_aiParamsPath = path; }

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
    render::UnitRenderer   m_unitRenderer;
    sim::WorldMap          m_worldMap{128, 96, 32}; // 128x96 ô, mỗi ô 32x32
    sim::WorldSim          m_worldSim{m_worldMap};

    mapgen::MapGenerator   m_mapGenerator;
    mapgen::MapGenConfig   m_mapConfig;
    uint64_t               m_currentSeed{12345678ULL};

    bool                   m_isDraggingMinimap{false};
    float                  m_simAccumulator{0.0f};

    // Thông báo hiển thị tạm trên màn hình (ví dụ: "Seed moi: <seed>")
    std::string            m_notificationText;
    float                  m_notificationTimer{0.0f};

    // Biến đo lường và tính toán FPS
    int   m_frameCount{0};
    float m_fpsAccumulator{0.0f};
    float m_currentFPS{0.0f};

    // Trạng thái chọn quân chuột (M4)
    bool     m_isBoxSelecting{false};
    int      m_boxStartX{0};
    int      m_boxStartY{0};
    int      m_boxCurrentX{0};
    int      m_boxCurrentY{0};
    uint32_t m_inspectedUnitId{0};

    // Chế độ Debug phím F2 bật/tắt sương mù (M6)
    // TODO: [DEBUG ONLY - F2] Xóa chế độ tắt sương mù này khi hoàn thiện game
    bool     m_debugFogDisabled{false};

    // AI chiến thuật phe Enemy (M7/M9)
    ai::TacticalAI m_enemyAI{sim::Faction::Enemy, 12345678ULL};
    bool           m_aiEnabled{true};
    std::string    m_aiParamsPath;
    std::string    m_aiSourceLabel{"AI: Default"};

    // Hệ thống Replay (M8)
    std::string          m_replayPath;
    bool                 m_isReplayMode{false};
    bool                 m_replayPaused{false};
    float                m_replaySpeed{1.0f};
    replay::ReplayPlayer m_replayPlayer;
};

} // namespace app
