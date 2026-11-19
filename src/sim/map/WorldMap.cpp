#include "WorldMap.hpp"

#include <algorithm>

namespace sim {

WorldMap::WorldMap(int width, int height, int tileSize)
    : m_width(width)
    , m_height(height)
    , m_tileSize(tileSize)
    , m_tiles(static_cast<size_t>(width * height), TerrainType::Plain)
{
}

int WorldMap::getWidth() const {
    return m_width;
}

int WorldMap::getHeight() const {
    return m_height;
}

int WorldMap::getTileSize() const {
    return m_tileSize;
}

int WorldMap::getWorldWidth() const {
    return m_width * m_tileSize;
}

int WorldMap::getWorldHeight() const {
    return m_height * m_tileSize;
}

int WorldMap::getTotalTiles() const {
    return m_width * m_height;
}

bool WorldMap::isInBounds(int tileX, int tileY) const {
    return (tileX >= 0 && tileX < m_width && tileY >= 0 && tileY < m_height);
}

TerrainType WorldMap::getTile(int tileX, int tileY) const {
    if (!isInBounds(tileX, tileY)) {
        return TerrainType::Sea;
    }
    return m_tiles[static_cast<size_t>(tileY * m_width + tileX)];
}

void WorldMap::setTile(int tileX, int tileY, TerrainType type) {
    if (isInBounds(tileX, tileY)) {
        m_tiles[static_cast<size_t>(tileY * m_width + tileX)] = type;
    }
}

TerrainType WorldMap::getTileByIndex(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < m_tiles.size()) {
        return m_tiles[static_cast<size_t>(index)];
    }
    return TerrainType::Sea;
}

void WorldMap::setTileByIndex(int index, TerrainType type) {
    if (index >= 0 && static_cast<size_t>(index) < m_tiles.size()) {
        m_tiles[static_cast<size_t>(index)] = type;
    }
}

const std::vector<TerrainType>& WorldMap::getTiles() const {
    return m_tiles;
}

void WorldMap::resize(int width, int height, int tileSize) {
    m_width = width;
    m_height = height;
    m_tileSize = tileSize;
    m_tiles.assign(static_cast<size_t>(width * height), TerrainType::Plain);
}

void WorldMap::fill(TerrainType type) {
    std::fill(m_tiles.begin(), m_tiles.end(), type);
}

int WorldMap::getTileCount(TerrainType type) const {
    int count = 0;
    for (TerrainType t : m_tiles) {
        if (t == type) {
            count++;
        }
    }
    return count;
}

int WorldMap::getSeaCount() const {
    return getTileCount(TerrainType::Sea);
}

int WorldMap::getLandCount() const {
    return getTotalTiles() - getSeaCount();
}

uint64_t WorldMap::computeHash() const {
    // Thuật toán FNV-1a 64-bit băm toàn bộ dữ liệu địa hình
    uint64_t hash = 14695981039346656037ULL;
    for (TerrainType t : m_tiles) {
        hash ^= static_cast<uint8_t>(t);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace sim
