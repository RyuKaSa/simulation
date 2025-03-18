#include "BlockWorld.hpp"
#include "Simulation.hpp" // For SimulationBase and its getters.
#include <cmath>
#include <iostream>

BlockWorld::BlockWorld(float gridSpacing, float gridExtent, SimulationBase* sim)
    : m_gridSpacing(gridSpacing), m_gridExtent(gridExtent), simulation(sim)
{
    std::cout << "BlockWorld created with gridSpacing: " << m_gridSpacing 
              << ", gridExtent: " << m_gridExtent << std::endl;
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
    
    // Record the block.
    m_blocks.insert(coord);

    // Determine the world position of the block.
    glm::vec3 worldPos = gridToWorld(coord);
    // Get the simulation's particle data.
    ParticleSoA &soa = simulation->getSoAReference();
    
    // Add a new external particle (push_back) to the simulation.
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

    // Record mapping from grid coordinate to the new particle index.
    m_blockIndex[coord] = newIndex;
    std::cout << "Block added at (" << cx << ", " << cy << ", " << cz 
              << ") with new particle index " << newIndex << std::endl;
    return true;
}

bool BlockWorld::removeBlock(int cx, int cy, int cz) {
    GridCoord coord {cx, cy, cz};
    if (m_blocks.erase(coord) > 0) {
        auto it = m_blockIndex.find(coord);
        if (it != m_blockIndex.end()) {
            size_t particleIndex = it->second;
            // Remove the external particle from the simulation.
            ParticleSoA &soa = simulation->getSoAReference();
            std::vector<SpringData> &springs = simulation->getSpringsReference();
            removeParticle(soa, springs, particleIndex); // Defined as an inline helper in SimulationSoAInternals.hpp

            // Erase the mapping.
            m_blockIndex.erase(it);

            // Update indices for all external particles with indices greater than the removed one.
            for (auto &pair : m_blockIndex) {
                if (pair.second > particleIndex) {
                    pair.second--;
                }
            }
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
    // Return the center position of the grid cell.
    return glm::vec3(coord.x * m_gridSpacing, coord.y * m_gridSpacing, coord.z * m_gridSpacing);
}

void BlockWorld::updateSimulation(ParticleSoA &soa, std::vector<SpringData> &springs) const {
    // For each external block, update its particle position.
    for (const auto &pair : m_blockIndex) {
        const GridCoord &coord = pair.first;
        size_t index = pair.second;
        glm::vec3 worldPos = gridToWorld(coord);
        if (index < soa.position.size()) {
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
    // Reorder the SoA so that cloth particles come first.
    reorderClothFirst(soa, springs);
}