#include "WorldMap.hpp"

namespace sim {

WorldMap::WorldMap(int width, int height, int tileSize)
    : m_width(width)
    , m_height(height)
    , m_tileSize(tileSize)
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

bool WorldMap::isInBounds(int tileX, int tileY) const {
    return (tileX >= 0 && tileX < m_width && tileY >= 0 && tileY < m_height);
}

} // namespace sim
