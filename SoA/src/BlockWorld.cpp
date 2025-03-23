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

    // Optionally print the updated list of blocks
    printBlockList();

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
    // Optionally print the updated list of blocks
    printBlockList();

    return true;
}

bool BlockWorld::addBlockForce(int cx, int cy, int cz)
{
    if (cy < 0)
        return false; // still don't allow below ground

    GridCoord coord {cx, cy, cz};
    if (m_blocks.find(coord) != m_blocks.end())
        return false; // block already exists

    // Bypass adjacency check
    m_blocks.insert(coord);

    glm::vec3 worldPos = gridToWorld(coord);
    ParticleSoA &soa = simulation->getSoAReference();
    size_t newIndex = soa.position.size();

    soa.position.push_back(glm::dvec3(worldPos));
    soa.velocity.push_back(glm::dvec3(0.0));
    soa.forceAccum.push_back(glm::dvec3(0.0));
    soa.mass.push_back(1.0);
    soa.type.push_back(ParticleType::BACKGROUND);
    soa.isStatic.push_back(true);
    soa.color.push_back(glm::dvec3(1.0, 1.0, 1.0));
    soa.dimensions.push_back(glm::dvec3(m_gridSpacing));
    soa.clothID.push_back(-1);

    m_blockIndex[coord] = newIndex;
    return true;
}

void BlockWorld::placeHollowCube(const std::vector<GridCoord>& corners)
{
    if (corners.size() < 8)
    {
        std::cerr << "placeHollowCube: Not enough corners provided!" << std::endl;
        return;
    }
    
    // Determine the bounding box from the 8 corners.
    int minX = corners[0].x, maxX = corners[0].x;
    int minY = corners[0].y, maxY = corners[0].y;
    int minZ = corners[0].z, maxZ = corners[0].z;
    for (const auto &c : corners)
    {
        if (c.x < minX) minX = c.x;
        if (c.x > maxX) maxX = c.x;
        if (c.y < minY) minY = c.y;
        if (c.y > maxY) maxY = c.y;
        if (c.z < minZ) minZ = c.z;
        if (c.z > maxZ) maxZ = c.z;
    }
    
    // Loop through every coordinate in the cuboid.
    // Add a block only if the coordinate is on the boundary.
    // (A coordinate is on the boundary if any of its components equals the min or max.)
    for (int x = minX; x <= maxX; x++)
    {
        for (int y = minY; y <= maxY; y++)
        {
            for (int z = minZ; z <= maxZ; z++)
            {
                if (x == minX || x == maxX ||
                    y == minY || y == maxY ||
                    z == minZ || z == maxZ)
                {
                    // Here we use addBlock. If the adjacency check is an issue,
                    // you might consider an alternate method that bypasses it.
                    addBlock(x, y, z);
                }
            }
        }
    }
}

void BlockWorld::printBlockList() const {
    std::vector<BlockInfo> blocks = getBlockList();
    std::cout << "Current blocks (" << blocks.size() << "):\n";
    for (const auto &info : blocks) {
        std::cout << "Index: " << info.index
                  << " | Grid: (" << info.grid.x << ", " << info.grid.y << ", " << info.grid.z << ")"
                  << " | World: (" << info.world.x << ", " << info.world.y << ", " << info.world.z << ")\n";
    }
}

std::vector<BlockInfo> BlockWorld::getBlockList() const {
    std::vector<BlockInfo> blockList;
    for (const auto &entry : m_blockIndex) {
        BlockInfo info;
        info.index = entry.second;
        info.grid = entry.first;
        info.world = gridToWorld(entry.first);
        blockList.push_back(info);
    }
    return blockList;
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

void BlockWorld::clearAllBlocks()
{
    ParticleSoA &soa = simulation->getSoAReference();
    std::vector<SpringData> &springs = simulation->getSpringsReference();

    // Gather all block particle indices from m_blockIndex.
    std::vector<size_t> indices;
    for (const auto &entry : m_blockIndex) {
        indices.push_back(entry.second);
    }
    // Sort in descending order so that removing one does not affect the indices of those with lower values.
    std::sort(indices.begin(), indices.end(), std::greater<size_t>());

    // Remove each block using the robust removeParticle() call.
    for (size_t idx : indices) {
        // removeBlock(...) calls removeParticle() internally.
        // We use the overload that takes GridCoord, so we need to recover the coordinate.
        // Alternatively, if you have a direct way to call removeParticle for a given index, do so.
        // Here, we iterate over a copy of m_blockIndex.
        // (Since removeBlock() also updates m_blockIndex, we rely on this descending order removal.)
        for (auto it = m_blockIndex.begin(); it != m_blockIndex.end(); ) {
            if (it->second == idx) {
                // Remove the block at this coordinate.
                removeBlock(it->first);
                // erase returns the next iterator.
                it = m_blockIndex.begin();  // start over since m_blockIndex has been modified
            } else {
                ++it;
            }
        }
    }

    // After all removals, clear our bookkeeping.
    m_blocks.clear();
    m_blockIndex.clear();

    std::cout << "All blocks have been cleared.\n";
}