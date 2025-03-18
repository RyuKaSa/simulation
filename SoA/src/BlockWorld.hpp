#ifndef BLOCKWORLD_HPP
#define BLOCKWORLD_HPP

#include <unordered_set>
#include <unordered_map>
#include <glm/glm.hpp>
#include "SimulationSoAInternals.hpp" // Defines ParticleSoA and SpringData, plus reorder functions.

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
    // The constructor now takes a pointer to the simulation.
    BlockWorld(float gridSpacing, float gridExtent, SimulationBase* sim);

    // Adds a block at grid cell (cx, cy, cz). Returns true if placement is successful.
    bool addBlock(int cx, int cy, int cz);

    // Removes a block at grid cell (cx, cy, cz). Returns true if removal is successful.
    bool removeBlock(int cx, int cy, int cz);

    // Checks if a block exists at grid cell (cx, cy, cz).
    bool hasBlock(int cx, int cy, int cz) const;

    // Converts a world position to grid coordinates.
    GridCoord worldToGrid(float wx, float wy, float wz) const;

    // Converts grid coordinates to world-space position (cell center).
    glm::vec3 gridToWorld(const GridCoord &coord) const;

    // Updates the simulation’s external particles with current block data.
    void updateSimulation(ParticleSoA &soa, std::vector<SpringData> &springs) const;

    float getGridSpacing() const { return m_gridSpacing; }
    float getGridExtent() const { return m_gridExtent; }

private:
    float m_gridSpacing;
    float m_gridExtent;
    SimulationBase* simulation; // Pointer to the simulation

    // Set of active blocks.
    std::unordered_set<GridCoord, GridCoordHash> m_blocks;
    // Mapping from grid coordinate to external particle index.
    std::unordered_map<GridCoord, size_t, GridCoordHash> m_blockIndex;
};

#endif // BLOCKWORLD_HPP