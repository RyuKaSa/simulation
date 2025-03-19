#ifndef BLOCKWORLD_HPP
#define BLOCKWORLD_HPP

#include <unordered_set>
#include <unordered_map>
#include <glm/glm.hpp>
#include "SimulationSoAInternals.hpp" // ParticleSoA, etc.

// Structure to represent a grid cell coordinate.
struct GridCoord {
    int x, y, z;
    bool operator==(const GridCoord &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash functor for GridCoord.
struct GridCoordHash {
    std::size_t operator()(const GridCoord &coord) const {
        std::size_t hx = std::hash<int>()(coord.x);
        std::size_t hy = std::hash<int>()(coord.y);
        std::size_t hz = std::hash<int>()(coord.z);
        return ((hx ^ (hy << 1)) >> 1) ^ (hz << 1);
    }
};

class SimulationBase; // Forward declaration

// BlockWorld class manages block placement on a 3D grid.
class BlockWorld {
public:
    BlockWorld(float gridSpacing, float gridExtent, SimulationBase* sim);

    bool addBlock(int cx, int cy, int cz);
    
    // Overload to remove by ints or by GridCoord:
    bool removeBlock(int cx, int cy, int cz);
    bool removeBlock(const GridCoord &coord) {
        return removeBlock(coord.x, coord.y, coord.z);
    }

    // Check if block exists
    bool hasBlock(int cx, int cy, int cz) const;
    bool hasBlock(const GridCoord &coord) const {
        return hasBlock(coord.x, coord.y, coord.z);
    }

    // Convert between world coords and grid coords
    GridCoord worldToGrid(float wx, float wy, float wz) const;
    glm::vec3 gridToWorld(const GridCoord &coord) const;

    // Sync block data to the simulation SoA (positions, etc.).
    void updateSimulation(ParticleSoA &soa, std::vector<SpringData> &springs) const;

    float getGridSpacing() const { return m_gridSpacing; }
    float getGridExtent()  const { return m_gridExtent; }

    // *** NEW: fully clear all blocks from the scene. ***
    // Loops over all active blocks and removes them from SoA.
    void clearAllBlocks();

private:
    float m_gridSpacing;
    float m_gridExtent;
    SimulationBase* simulation; // pointer to the sim

    // Set of active blocks
    std::unordered_set<GridCoord, GridCoordHash> m_blocks;
    // Mapping from grid coord -> external particle index
    std::unordered_map<GridCoord, size_t, GridCoordHash> m_blockIndex;
};

#endif // BLOCKWORLD_HPP