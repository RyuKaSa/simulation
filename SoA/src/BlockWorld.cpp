#include "BlockWorld.hpp"
#include "SimulationSoAInternals.hpp" // Provides ParticleSoA and ParticleType.
#include <cmath>
#include <iostream>

BlockWorld::BlockWorld(float gridSpacing, float gridExtent, size_t poolSize)
    : m_gridSpacing(gridSpacing), m_gridExtent(gridExtent) {
    std::cout << "BlockWorld created with gridSpacing: " << m_gridSpacing 
              << ", gridExtent: " << m_gridExtent 
              << ", poolSize: " << poolSize << std::endl;
    // Pre-allocate free indices.
    for (size_t i = 0; i < poolSize; i++) {
        m_freeIndices.push(i);
    }
}

bool BlockWorld::addBlock(int cx, int cy, int cz) {
    if (cy < 0)
        return false; // Do not allow blocks below ground.
    GridCoord coord {cx, cy, cz};
    if (m_blocks.find(coord) != m_blocks.end())
        return false; // Block already exists.

    // Enforce adjacency if not on ground.
    if (cy > 0) {
        bool adjacent = false;
        GridCoord neighbors[6] = {
            {cx+1, cy, cz}, {cx-1, cy, cz},
            {cx, cy+1, cz}, {cx, cy-1, cz},
            {cx, cy, cz+1}, {cx, cy, cz-1}
        };
        for (const auto &n : neighbors) {
            if (m_blocks.find(n) != m_blocks.end()) {
                adjacent = true;
                break;
            }
        }
        if (!adjacent) {
            std::cout << "Block at (" << cx << ", " << cy << ", " << cz 
                      << ") is not adjacent. Placement denied." << std::endl;
            return false;
        }
    }
    // Add the block.
    m_blocks.insert(coord);
    if (m_freeIndices.empty()) {
        std::cout << "No free block indices available!" << std::endl;
        return false;
    }
    size_t poolIndex = m_freeIndices.front();
    m_freeIndices.pop();
    m_blockIndex[coord] = poolIndex;
    std::cout << "Block added at (" << cx << ", " << cy << ", " << cz 
              << ") with pool index " << poolIndex << std::endl;
    return true;
}

bool BlockWorld::removeBlock(int cx, int cy, int cz) {
    GridCoord coord {cx, cy, cz};
    if (m_blocks.erase(coord) > 0) {
        auto it = m_blockIndex.find(coord);
        if (it != m_blockIndex.end()) {
            size_t poolIndex = it->second;
            m_freeIndices.push(poolIndex);
            m_blockIndex.erase(it);
        }
        std::cout << "Block removed at (" << cx << ", " << cy << ", " << cz << ")." << std::endl;
        return true;
    }
    return false;
}

bool BlockWorld::hasBlock(int cx, int cy, int cz) const {
    GridCoord coord {cx, cy, cz};
    return (m_blocks.find(coord) != m_blocks.end());
}

GridCoord BlockWorld::worldToGrid(float wx, float wy, float wz) const {
    int cx = static_cast<int>(std::floor(wx / m_gridSpacing + 0.5f));
    int cy = static_cast<int>(std::floor(wy / m_gridSpacing + 0.5f));
    int cz = static_cast<int>(std::floor(wz / m_gridSpacing + 0.5f));
    return GridCoord {cx, cy, cz};
}

glm::vec3 BlockWorld::gridToWorld(const GridCoord &coord) const {
    // Returns the center position of the cell.
    return glm::vec3(coord.x * m_gridSpacing, coord.y * m_gridSpacing, coord.z * m_gridSpacing);
}

void BlockWorld::updateSimulation(ParticleSoA &soa) const {
    // We assume that the external block pool is allocated at the end of the SoA.
    // Let poolStart be the starting index for block particles.
    // (For example, if SoA originally had N particles and we pre-allocated poolSize additional particles,
    // then poolStart = N.)
    size_t totalParticles = soa.position.size();
    // We assume poolSize = (m_freeIndices.size() + m_blockIndex.size()).
    size_t poolSize = m_freeIndices.size() + m_blockIndex.size();
    size_t poolStart = totalParticles - poolSize;
    
    // For each active block, update its corresponding external particle.
    for (const auto &pair : m_blockIndex) {
        const GridCoord &coord = pair.first;
        size_t poolIndex = pair.second;
        glm::vec3 worldPos = gridToWorld(coord);
        size_t index = poolStart + poolIndex;
        if (index < soa.position.size()) {
            soa.position[index]    = glm::dvec3(worldPos);
            soa.velocity[index]    = glm::dvec3(0.0);
            soa.forceAccum[index]  = glm::dvec3(0.0);
            soa.mass[index]        = 1.0;
            soa.type[index]        = ParticleType::EXTERNAL;
            soa.isStatic[index]    = true;
            soa.color[index]       = glm::dvec3(1.0, 1.0, 1.0);
            soa.dimensions[index]  = glm::dvec3(m_gridSpacing);
        }
    }
}