#pragma once

#include "map/WorldMap.hpp"
#include "units/Unit.hpp"
#include "commands/Command.hpp"
#include "config/SimConfig.hpp"
#include "combat/CombatSystem.hpp"
#include "../mapgen/FastRng.hpp"

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <atomic>

namespace sim {

/**
 * @brief Chế độ thứ tự ưu tiên xử lý giao chiến trong WorldSim (M9.10)
 */
enum class CombatOrderMode {
    PlayerFirst = 0,    // Mặc định: ID tăng dần (1..60)
    EnemyFirst = 1,     // ID giảm dần (60..1)
    AlternateTicks = 2, // Chẵn: PlayerFirst, Lẻ: EnemyFirst
    Simultaneous = 3    // Thu thập sát thương rồi áp dụng cùng lúc
};

/**
 * @brief Lớp WorldSim quản lý toàn bộ trạng thái mô phỏng game (thuần C++, không SDL, không float).
 */
class WorldSim {
public:
    explicit WorldSim(WorldMap& map);
    ~WorldSim() = default;

    void setCombatOrderMode(CombatOrderMode mode) { m_combatOrderMode = mode; }
    [[nodiscard]] CombatOrderMode getCombatOrderMode() const { return m_combatOrderMode; }

    // Nạp cấu hình từ data/game_config.json
    bool loadConfig();

    // Sinh 30 chấm cho mỗi bên (Player phía Tây, Enemy phía Đông, đối xứng gương)
    void spawnDivisions(uint64_t seed);
    void spawnPlayerDivisions(uint64_t seed) { spawnDivisions(seed); }

    // Gửi lệnh vào hàng đợi mô phỏng (áp dụng ở đầu tick tiếp theo)
    void queueCommand(const Command& cmd);

    // Tiện ích gửi lệnh di chuyển cho danh sách đơn vị tới ô đích
    void queueMoveCommand(const std::vector<uint32_t>& unitIds, int targetTileX, int targetTileY);

    // Hook lắng nghe các lệnh được gửi vào WorldSim (cho ReplayRecorder)
    using CommandHook = std::function<void(uint64_t tick, const Command& cmd)>;
    void setCommandHook(CommandHook hook) { m_commandHook = std::move(hook); }

    // Chế độ im lặng (không in log sinh quân WorldSim ra stdout)
    void setQuiet(bool q) { m_quiet = q; }
    [[nodiscard]] bool isQuiet() const { return m_quiet; }
    static void setGlobalQuiet(bool q) { s_globalQuiet.store(q); }
    static bool isGlobalQuiet() { return s_globalQuiet.load(); }

    // Thực hiện 1 bước mô phỏng cố định (1 tick)
    void tick();

    // Truy cập dữ liệu trạng thái để hiển thị (chỉ đọc)
    [[nodiscard]] const std::vector<Unit>& getUnits() const { return m_units; }
    [[nodiscard]] std::vector<Unit>& getUnits() { return m_units; }

    [[nodiscard]] const Unit* getUnitById(uint32_t id) const;
    [[nodiscard]] Unit* getUnitById(uint32_t id);

    [[nodiscard]] const SimConfig& getConfig() const { return m_config; }
    [[nodiscard]] SimConfig& getConfig() { return m_config; }
    [[nodiscard]] const WorldMap& getMap() const { return m_map; }
    [[nodiscard]] uint64_t getCurrentTick() const { return m_currentTick; }
    void setCurrentTick(uint64_t tick) { m_currentTick = tick; }

    // Chiến đấu & Kết quả ván đấu (M5)
    [[nodiscard]] bool isGameOver() const { return m_matchEndInfo.result != MatchResult::InProgress; }
    [[nodiscard]] const MatchEndInfo& getMatchEndInfo() const { return m_matchEndInfo; }
    [[nodiscard]] const std::vector<Tracer>& getActiveTracers() const { return m_activeTracers; }
    [[nodiscard]] uint64_t getMatchDurationTicks() const {
        return static_cast<uint64_t>(m_config.getMatchDurationSec()) * m_config.getTickRate();
    }
    [[nodiscard]] int getRemainingTimeSec() const;

    // Chế độ Debug phím F1 (M5)
    [[nodiscard]] Faction getControlledFaction() const { return m_controlledFaction; }
    void setControlledFaction(Faction f) { m_controlledFaction = f; }
    void toggleControlledFaction() {
        m_controlledFaction = (m_controlledFaction == Faction::Player) ? Faction::Enemy : Faction::Player;
    }

    // Tọa độ trung điểm neo xuất hiện quân (Spawn Anchors)
    [[nodiscard]] int getPlayerSpawnAnchorX() const { return m_playerSpawnAnchorX; }
    [[nodiscard]] int getPlayerSpawnAnchorY() const { return m_playerSpawnAnchorY; }
    [[nodiscard]] int getEnemySpawnAnchorX() const { return m_enemySpawnAnchorX; }
    [[nodiscard]] int getEnemySpawnAnchorY() const { return m_enemySpawnAnchorY; }

    // Sương mù chiến tranh & Tầm nhìn (M6)
    void updateVision();
    void initElevationGrid();
    [[nodiscard]] const std::vector<uint8_t>& getElevationGrid() const { return m_elevationGrid; }
    [[nodiscard]] DetailedTerrainType getDetailedTerrain(int x, int y) const;
    [[nodiscard]] const std::vector<uint8_t>& getFogMap(Faction f) const {
        return (f == Faction::Player) ? m_fogPlayer : m_fogEnemy;
    }
    [[nodiscard]] bool isTileVisible(int x, int y, Faction f) const;
    [[nodiscard]] bool isUnitVisible(const Unit& u, Faction observerFaction) const;
    [[nodiscard]] double getLastVisionUpdateTimeMs() const { return m_lastVisionUpdateTimeMs; }

    // Các hàm xử lý nội bộ
    void resolveCombat();
    void checkMatchConditions();

private:
    WorldMap&            m_map;
    SimConfig            m_config;
    std::vector<Unit>    m_units;
    std::vector<Command> m_commandQueue;
    CommandHook          m_commandHook;
    uint64_t             m_currentTick{0};

    // Bộ sinh số ngẫu nhiên cho chiến đấu, lưu trữ trạng thái tất định
    mapgen::FastRng      m_combatRng{12345678ULL};

    // Danh sách đường đạn đang hiển thị
    std::vector<Tracer>  m_activeTracers;

    // Trạng thái kết thúc ván đấu
    MatchEndInfo         m_matchEndInfo;

    // Phe đang được điều khiển (Mặc định: Player, F1 đổi sang Enemy để debug)
    Faction              m_controlledFaction{Faction::Player};

    // Tọa độ trung điểm neo xuất hiện quân 2 bên bờ biển
    int                  m_playerSpawnAnchorX{10};
    int                  m_playerSpawnAnchorY{48};
    int                  m_enemySpawnAnchorX{117};
    int                  m_enemySpawnAnchorY{48};

    // Sương mù chiến tranh cho từng phe (M6)
    std::vector<uint8_t> m_elevationGrid;
    std::vector<uint8_t> m_detailedTerrainGrid;
    std::vector<uint8_t> m_fogPlayer;
    std::vector<uint8_t> m_fogEnemy;
    std::vector<uint16_t> m_visiblePlayerIndices;
    std::vector<uint16_t> m_visibleEnemyIndices;
    std::vector<uint32_t> m_aliveIdsScratch;
    double               m_lastVisionUpdateTimeMs{0.0};
    CombatOrderMode      m_combatOrderMode{CombatOrderMode::Simultaneous};

    bool                 m_quiet{false};
    static inline std::atomic<bool> s_globalQuiet{false};
};

} // namespace sim
