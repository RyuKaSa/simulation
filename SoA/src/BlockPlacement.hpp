#ifndef BLOCKPLACEMENT_HPP
#define BLOCKPLACEMENT_HPP

#include <glm/glm.hpp>
#include "BlockWorld.hpp"
#include "Simulation.hpp"

// Given a ray (rayOrigin and rayDir) and a BlockWorld instance,
// computes the grid coordinate for block placement by intersecting the ray with the ground plane,
// then snapping the hit point to the grid. Returns true if an intersection is found.
bool GetBlockPlacementPosition(const glm::vec3& rayOrigin, const glm::vec3& rayDir, 
                               const BlockWorld &world, const SimulationBase &sim, GridCoord &outCoord);

#endif // BLOCKPLACEMENT_HPP