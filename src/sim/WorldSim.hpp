#pragma once

#include "map/WorldMap.hpp"
#include "units/Unit.hpp"
#include "commands/Command.hpp"
#include "config/SimConfig.hpp"

#include <vector>
#include <cstdint>

namespace sim {

/**
 * @brief Lớp WorldSim quản lý toàn bộ trạng thái mô phỏng game (thuần C++, không SDL, không float).
 *
 * Chức năng:
 * - Tiếp nhận và lưu trữ các lệnh từ người chơi/AI vào hàng đợi (command queue).
 * - Thực hiện bước mô phỏng cố định (tick) theo quy tắc 20 tick/giây.
 * - Quản lý danh sách các sư đoàn quân đội (Unit).
 * - Khởi tạo 10 sư đoàn bộ binh tất định từ seed trên các ô đất bằng gần nhau.
 */
class WorldSim {
public:
    explicit WorldSim(WorldMap& map);
    ~WorldSim() = default;

    // Nạp cấu hình từ thư mục data/
    bool loadConfig();

    // Sinh 10 sư đoàn bộ binh người chơi theo cách tất định từ seed
    void spawnPlayerDivisions(uint64_t seed);

    // Gửi một lệnh vào hàng đợi mô phỏng (áp dụng ở đầu tick tiếp theo)
    void queueCommand(const Command& cmd);

    // Tiện ích gửi lệnh di chuyển cho một danh sách đơn vị tới ô đích
    void queueMoveCommand(const std::vector<uint32_t>& unitIds, int targetTileX, int targetTileY);

    // Thực hiện 1 bước mô phỏng cố định (1 tick)
    void tick();

    // Truy cập dữ liệu trạng thái để hiển thị (chỉ đọc)
    [[nodiscard]] const std::vector<Unit>& getUnits() const { return m_units; }
    [[nodiscard]] std::vector<Unit>& getUnits() { return m_units; }

    [[nodiscard]] const Unit* getUnitById(uint32_t id) const;
    [[nodiscard]] Unit* getUnitById(uint32_t id);

    [[nodiscard]] const SimConfig& getConfig() const { return m_config; }
    [[nodiscard]] const WorldMap& getMap() const { return m_map; }
    [[nodiscard]] uint64_t getCurrentTick() const { return m_currentTick; }

private:
    WorldMap&            m_map;
    SimConfig            m_config;
    std::vector<Unit>    m_units;
    std::vector<Command> m_commandQueue;
    uint64_t             m_currentTick{0};
};

} // namespace sim
