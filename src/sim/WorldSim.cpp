#include "WorldSim.hpp"
#include "../mapgen/FastRng.hpp"

#include <iostream>
#include <queue>
#include <vector>

namespace sim {

WorldSim::WorldSim(WorldMap& map)
    : m_map(map) {}

bool WorldSim::loadConfig() {
    return m_config.autoLoad();
}

void WorldSim::queueCommand(const Command& cmd) {
    m_commandQueue.push_back(cmd);
}

void WorldSim::queueMoveCommand(const std::vector<uint32_t>& unitIds, int targetTileX, int targetTileY) {
    Command cmd;
    cmd.type = CommandType::Move;
    cmd.move.unitIds = unitIds;
    cmd.move.targetTileX = targetTileX;
    cmd.move.targetTileY = targetTileY;
    queueCommand(cmd);
}

void WorldSim::spawnPlayerDivisions(uint64_t seed) {
    m_units.clear();
    m_commandQueue.clear();

    int width = m_map.getWidth();
    int height = m_map.getHeight();

    // Dùng PRNG tất định từ seed để tìm cụm đất bằng
    mapgen::FastRng rng(seed ^ 0xFEEDFACECAFEBEEFULL);

    struct Coord {
        int x{0};
        int y{0};
    };

    std::vector<Coord> chosenTiles;
    constexpr int requiredUnits = 10;

    // Tìm kiếm vị trí neo (anchor) là ô đất bằng, xung quanh có đủ 10 ô đất bằng liên thông
    for (int attempt = 0; attempt < 500 && chosenTiles.size() < requiredUnits; ++attempt) {
        int anchorX = rng.range(12, width - 13);
        int anchorY = rng.range(12, height - 13);

        if (m_map.getTile(anchorX, anchorY) != TerrainType::Plain) {
            continue;
        }

        // Chạy BFS tìm các ô Plain lân cận với thứ tự duyệt cố định (Trên, Phải, Dưới, Trái)
        std::vector<bool> visited(static_cast<size_t>(width * height), false);
        std::queue<Coord> queue;
        std::vector<Coord> cluster;

        visited[static_cast<size_t>(anchorY * width + anchorX)] = true;
        queue.push({anchorX, anchorY});
        cluster.push_back({anchorX, anchorY});

        constexpr int dx[4] = {0, 1, 0, -1};
        constexpr int dy[4] = {-1, 0, 1, 0};

        while (!queue.empty() && cluster.size() < requiredUnits) {
            Coord cur = queue.front();
            queue.pop();

            for (int dir = 0; dir < 4; ++dir) {
                int nx = cur.x + dx[dir];
                int ny = cur.y + dy[dir];

                if (!m_map.isInBounds(nx, ny)) continue;
                size_t idx = static_cast<size_t>(ny * width + nx);
                if (visited[idx]) continue;
                visited[idx] = true;

                if (m_map.getTile(nx, ny) == TerrainType::Plain) {
                    cluster.push_back({nx, ny});
                    queue.push({nx, ny});
                    if (cluster.size() >= requiredUnits) {
                        break;
                    }
                }
            }
        }

        if (cluster.size() >= requiredUnits) {
            chosenTiles = std::move(cluster);
            break;
        }
    }

    // Nếu không tìm đủ cụm qua BFS ngẫu nhiên, quét tuyến tính tất định trên bản đồ
    if (chosenTiles.size() < requiredUnits) {
        chosenTiles.clear();
        for (int y = 0; y < height && chosenTiles.size() < requiredUnits; ++y) {
            for (int x = 0; x < width && chosenTiles.size() < requiredUnits; ++x) {
                if (m_map.getTile(x, y) == TerrainType::Plain) {
                    chosenTiles.push_back({x, y});
                }
            }
        }
    }

    // Khởi tạo 10 sư đoàn bộ binh cho phe người chơi
    uint32_t nextId = 1;
    for (size_t i = 0; i < chosenTiles.size() && i < requiredUnits; ++i) {
        int tx = chosenTiles[i].x;
        int ty = chosenTiles[i].y;
        int32_t milliX = tx * 1000 + 500;
        int32_t milliY = ty * 1000 + 500;
        m_units.emplace_back(nextId++, Faction::Player, UnitType::InfantryDivision, milliX, milliY, m_config.getInfantry().manpower);
    }

    std::cout << "[WorldSim] Da dat " << m_units.size() << " su doan bo binh tat dinh tu seed " << seed << std::endl;
}

const Unit* WorldSim::getUnitById(uint32_t id) const {
    for (const auto& u : m_units) {
        if (u.getId() == id) return &u;
    }
    return nullptr;
}

Unit* WorldSim::getUnitById(uint32_t id) {
    for (auto& u : m_units) {
        if (u.getId() == id) return &u;
    }
    return nullptr;
}

void WorldSim::tick() {
    // 1. Áp dụng các lệnh từ hàng đợi ở đầu tick
    for (const auto& cmd : m_commandQueue) {
        if (cmd.type == CommandType::Move) {
            for (uint32_t uid : cmd.move.unitIds) {
                Unit* unit = getUnitById(uid);
                if (unit) {
                    unit->setDestinationTile(cmd.move.targetTileX, cmd.move.targetTileY);
                }
            }
        }
    }
    m_commandQueue.clear();

    // 2. Cập nhật di chuyển cho từng sư đoàn
    for (auto& unit : m_units) {
        if (!unit.hasDestination()) {
            continue;
        }

        // Lấy loại địa hình của ô đơn vị đang đứng
        TerrainType currentTerrain = m_map.getTile(unit.getTileX(), unit.getTileY());

        // Tính toán quãng đường di chuyển trong tick này dựa trên hệ số địa hình
        int stepMilli = m_config.calculateStepMilli(currentTerrain, unit.getMoveAccumulator());

        if (stepMilli > 0) {
            unit.stepTowardsDestination(stepMilli);
        }
    }

    m_currentTick++;
}

} // namespace sim
