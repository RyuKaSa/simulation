#ifndef BLOCKWORLD_HPP
#define BLOCKWORLD_HPP

#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <glm/glm.hpp>

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

// BlockWorld class manages block placement on a 3D grid and uses pre-allocated external particles.
class BlockWorld {
public:
    // Constructor:
    //   gridSpacing: size of one grid cell (and cube side)
    //   gridExtent: half-extent of the grid (for rendering purposes)
    //   poolSize: number of external particles pre-allocated for blocks.
    BlockWorld(float gridSpacing, float gridExtent, size_t poolSize);

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

    // Updates the simulation’s pre-allocated external particles with current block data.
    // (This writes into the ParticleSoA provided.)
    void updateSimulation(class ParticleSoA &soa) const;

    // Getters.
    float getGridSpacing() const { return m_gridSpacing; }
    float getGridExtent() const { return m_gridExtent; }

private:
    float m_gridSpacing;
    float m_gridExtent;
    // Set of active blocks.
    std::unordered_set<GridCoord, GridCoordHash> m_blocks;
    // Mapping from grid coordinate to pre-allocated pool index.
    std::unordered_map<GridCoord, size_t, GridCoordHash> m_blockIndex;
    // Free pool indices.
    std::queue<size_t> m_freeIndices;
};

#endif // BLOCKWORLD_HPP