#pragma once

#include "AiParams.hpp"
#include "AiTelemetry.hpp"
#include "../sim/WorldSim.hpp"
#include "../sim/commands/Command.hpp"
#include "../sim/units/Unit.hpp"
#include "../mapgen/FastRng.hpp"

#include <vector>
#include <cstdint>
#include <unordered_map>

namespace ai {

/**
 * @brief Bản ghi thông tin mục tiêu địch từng được phát hiện trong tầm nhìn (Fog of War Memory)
 */
struct EnemyMemoryRecord {
    uint32_t        unitId{0};
    sim::Faction    faction{sim::Faction::Neutral};
    sim::UnitType   type{sim::UnitType::Infantry};
    sim::UnitDomain domain{sim::UnitDomain::Land};
    int             lastTileX{0};
    int             lastTileY{0};
    uint64_t        lastSeenTick{0};
    int             lastHp{0};
    int             maxHp{0};
    int             armor{0};
};

/**
 * @brief Lớp TacticalAI đảm nhiệm trí tuệ nhân tạo chiến thuật cho một phe phái (M7)
 *
 * Đặc điểm kiến trúc:
 * - Thuần logic C++20, không phụ thuộc vào bất kỳ thư viện hiển thị nào (SDL2/Render).
 * - Tuân thủ tuyệt đối sương mù chiến tranh (Fog of War): chỉ quan sát qua tầm nhìn hợp lệ.
 * - Chỉ giao tiếp và ra quyết định thông qua struct Command chuẩn của WorldSim.
 * - Tất định 100% nhờ bộ sinh số FastRng với seed cố định.
 * - Hỗ trợ đầy đủ tham số AiParams để chuẩn bị cho M8 (Headless AI vs AI) và M9 (Tự tối ưu).
 */
class TacticalAI {
public:
    TacticalAI(sim::Faction controlledFaction, uint64_t seed = 42ULL, const AiParams& params = AiParams{});
    ~TacticalAI() = default;

    // Lập kế hoạch chiến thuật cho tick hiện tại, trả về danh sách các lệnh di chuyển/tác chiến
    [[nodiscard]] std::vector<sim::Command> plan(const sim::WorldSim& sim);

    // Tiện ích: tự động gọi plan() và đưa các lệnh vào hàng đợi lệnh của WorldSim
    void update(sim::WorldSim& sim);

    // Đặt lại trạng thái AI và seed mới khi bắt đầu ván đấu mới
    void reset(uint64_t newSeed);

    // Quản lý tham số và phe phái
    [[nodiscard]] const AiParams& getParams() const { return m_params; }
    void setParams(const AiParams& p) { m_params = p; }
    [[nodiscard]] sim::Faction getFaction() const { return m_controlledFaction; }
    [[nodiscard]] uint64_t getLastDecisionTick() const { return m_lastDecisionTick; }
    [[nodiscard]] const std::vector<EnemyMemoryRecord>& getMemory() const { return m_memory; }
    [[nodiscard]] const AiThoughtSnapshot& getSnapshot() const { return m_lastSnapshot; }
    [[nodiscard]] uint32_t getTotalCommandsIssued() const { return m_totalCommandsIssued; }
    [[nodiscard]] std::string getUnitIntent(uint32_t unitId) const {
        auto it = m_unitIntents.find(unitId);
        if (it != m_unitIntents.end()) return it->second;
        return "Idle/Standing";
    }

private:
    // Cập nhật bộ nhớ tình báo từ các đơn vị địch nhìn thấy được
    void updateEnemyMemory(const sim::WorldSim& sim);

    // Xử lý các quân chủng chi tiết
    void planCommander(const sim::WorldSim& sim, const sim::Unit& commander,
                       const std::vector<const sim::Unit*>& friendlyUnits,
                       std::vector<sim::Command>& outCommands);

    void planInfantryAndArmor(const sim::WorldSim& sim,
                             const std::vector<const sim::Unit*>& strikeUnits,
                             const sim::Unit* commander,
                             std::vector<sim::Command>& outCommands);

    void planArtilleryAndAntiAir(const sim::WorldSim& sim,
                                const std::vector<const sim::Unit*>& caesars,
                                const std::vector<const sim::Unit*>& gepards,
                                const sim::Unit* commander,
                                const std::vector<const sim::Unit*>& mainForce,
                                std::vector<sim::Command>& outCommands);

    void planAirForce(const sim::WorldSim& sim,
                     const std::vector<const sim::Unit*>& su57s,
                     const std::vector<const sim::Unit*>& b2s,
                     std::vector<sim::Command>& outCommands);

    void planNavy(const sim::WorldSim& sim,
                 const std::vector<const sim::Unit*>& ships,
                 std::vector<sim::Command>& outCommands);

    // Các hàm phụ trợ tính toán vị trí và chiến thuật
    [[nodiscard]] sim::TileCoord findSafeRetreatTile(const sim::WorldSim& sim,
                                                    int currentTileX, int currentTileY,
                                                    int threatCenterX, int threatCenterY,
                                                    sim::UnitDomain domain, sim::UnitType type,
                                                    int minRadius = 3, int maxRadius = 7);

    [[nodiscard]] sim::TileCoord findArtilleryStandOffTile(const sim::WorldSim& sim,
                                                          const sim::Unit& artillery,
                                                          int targetTileX, int targetTileY,
                                                          float idealRangeTiles);

    [[nodiscard]] sim::TileCoord pickScoutingTarget(const sim::WorldSim& sim,
                                                   int scoutIndex, int totalScouts,
                                                   sim::UnitDomain domain, sim::UnitType type);

    [[nodiscard]] const EnemyMemoryRecord* findHighValueEnemyTarget(sim::UnitDomain targetFilter = sim::UnitDomain::Land) const;

private:
    sim::Faction                   m_controlledFaction{sim::Faction::Enemy};
    AiParams                       m_params;
    mapgen::FastRng                m_rng{42ULL};
    uint64_t                       m_lastDecisionTick{0};
    bool                           m_isFirstDecision{true};
    std::vector<EnemyMemoryRecord> m_memory;
    AiThoughtSnapshot              m_lastSnapshot;
    uint32_t                       m_totalCommandsIssued{0};
    std::unordered_map<uint32_t, std::string> m_unitIntents;
};

} // namespace ai
