#include "BlockWorld.hpp"
#include "Simulation.hpp"
#include <cmath>
#include <iostream>

BlockWorld::BlockWorld(float gridSpacing, float gridExtent, SimulationBase* sim)
    : m_gridSpacing(gridSpacing), m_gridExtent(gridExtent), simulation(sim)
{
    std::cout << "BlockWorld created with gridSpacing: " << m_gridSpacing
              << ", gridExtent: " << m_gridExtent << std::endl;
}

bool BlockWorld::addBlock(int cx, int cy, int cz)
{
    if (cy < 0)
        return false; // do not allow blocks below ground

    GridCoord coord {cx, cy, cz};
    if (m_blocks.find(coord) != m_blocks.end())
        return false; // block already exists

    // Enforce adjacency if not on ground:
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
                      << ") is not adjacent.  Placement denied.\n";
            return false;
        }
    }

    // Record the block
    m_blocks.insert(coord);

    // Determine the world position
    glm::vec3 worldPos = gridToWorld(coord);

    // Insert new external particle into the simulation
    ParticleSoA &soa = simulation->getSoAReference();
    size_t newIndex = soa.position.size();

    soa.position.push_back(glm::dvec3(worldPos));
    soa.velocity.push_back(glm::dvec3(0.0));
    soa.forceAccum.push_back(glm::dvec3(0.0));
    soa.mass.push_back(1.0);
    soa.type.push_back(ParticleType::EXTERNAL);
    soa.isStatic.push_back(true);
    soa.color.push_back(glm::dvec3(1.0, 1.0, 1.0));
    soa.dimensions.push_back(glm::dvec3(m_gridSpacing));
    soa.clothID.push_back(-1);

    m_blockIndex[coord] = newIndex;

    std::cout << "Block added at (" << cx << ", " << cy << ", " << cz
              << ") with new particle index " << newIndex << std::endl;
    return true;
}

bool BlockWorld::removeBlock(int cx, int cy, int cz)
{
    GridCoord coord {cx, cy, cz};
    auto itSet = m_blocks.find(coord);
    if (itSet == m_blocks.end()) {
        // No such block
        return false;
    }
    // Erase from the set
    m_blocks.erase(itSet);

    // Find the SoA index
    auto itMap = m_blockIndex.find(coord);
    if (itMap != m_blockIndex.end())
    {
        size_t particleIndex = itMap->second;

        // Remove the external particle from the simulation SoA:
        ParticleSoA &soa = simulation->getSoAReference();
        std::vector<SpringData> &springs = simulation->getSpringsReference();
        removeParticle(soa, springs, particleIndex);

        // Erase from map
        m_blockIndex.erase(itMap);

        // Fix the indices in m_blockIndex that were > particleIndex
        for (auto &pair : m_blockIndex) {
            if (pair.second > particleIndex) {
                pair.second--;
            }
        }
    }
    std::cout << "Block removed at (" << cx << ", " << cy << ", " << cz << ").\n";
    return true;
}

bool BlockWorld::hasBlock(int cx, int cy, int cz) const
{
    GridCoord coord {cx, cy, cz};
    return (m_blocks.find(coord) != m_blocks.end());
}

GridCoord BlockWorld::worldToGrid(float wx, float wy, float wz) const
{
    int cx = static_cast<int>(std::floor(wx / m_gridSpacing + 0.5f));
    int cy = static_cast<int>(std::floor(wy / m_gridSpacing + 0.5f));
    int cz = static_cast<int>(std::floor(wz / m_gridSpacing + 0.5f));
    return GridCoord {cx, cy, cz};
}

glm::vec3 BlockWorld::gridToWorld(const GridCoord &coord) const
{
    // Return center position of the cell
    return glm::vec3(coord.x * m_gridSpacing,
                     coord.y * m_gridSpacing,
                     coord.z * m_gridSpacing);
}

void BlockWorld::updateSimulation(ParticleSoA &soa,
                                  std::vector<SpringData> &springs) const
{
    // Keep the SoA in sync with our block coordinates
    // (i.e. re-assign positions, ensure they're static, etc.)
    for (const auto &pair : m_blockIndex)
    {
        const GridCoord &coord = pair.first;
        size_t index = pair.second;

        if (index < soa.position.size()) {
            glm::vec3 worldPos = gridToWorld(coord);

            soa.position[index]   = glm::dvec3(worldPos);
            soa.velocity[index]   = glm::dvec3(0.0);
            soa.forceAccum[index] = glm::dvec3(0.0);
            soa.mass[index]       = 1.0;
            soa.type[index]       = ParticleType::EXTERNAL;
            soa.isStatic[index]   = true;
            soa.color[index]      = glm::dvec3(1.0, 1.0, 1.0);
            soa.dimensions[index] = glm::dvec3(m_gridSpacing);
            soa.clothID[index]    = -1;
        }
    }
    // Reorder so cloth particles are first
    // reorderClothFirst(soa, springs);
}

// NEW: remove all active blocks from the world and SoA
void BlockWorld::clearAllBlocks()
{
    // We must remove each block individually so that SoA indices remain consistent
    // while we’re removing. We'll gather them in a separate container first.
    std::vector<GridCoord> allBlocks(m_blocks.begin(), m_blocks.end());

    // Then remove them one by one.
    for (const GridCoord &coord : allBlocks)
    {
        removeBlock(coord); // calls removeBlock(int,int,int) under the hood
    }
    // Now m_blocks and m_blockIndex should be fully empty
    std::cout << "All blocks have been cleared.\n";
}