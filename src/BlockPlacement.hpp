#ifndef BLOCKPLACEMENT_HPP
#define BLOCKPLACEMENT_HPP

#include <glm/glm.hpp>
#include "BlockWorld.hpp"
#include "Simulation.hpp"

// For adding a block: returns true if we hit an existing block or ground;
// outCoord is adjusted so that a newly placed block sits on the hit face or ground.
bool GetBlockAdditionCoord(const glm::vec3& rayOrigin,
                           const glm::vec3& rayDir,
                           const BlockWorld &world,
                           const SimulationBase &sim,
                           GridCoord &outCoord);

// For removing a block: returns true if we actually hit an existing block. 
// outCoord is the grid coordinate of that EXACT block we are looking at.
bool GetBlockRemovalCoord(const glm::vec3& rayOrigin,
                          const glm::vec3& rayDir,
                          const BlockWorld &world,
                          const SimulationBase &sim,
                          GridCoord &outCoord);

#endif // BLOCKPLACEMENT_HPP