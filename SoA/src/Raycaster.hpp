#ifndef RAYCASTER_HPP
#define RAYCASTER_HPP

#include <glm/glm.hpp>
#include "Simulation.hpp" // or whatever includes ParticleSoA, etc.

bool RaycastGround(const glm::vec3& rayOrigin,
                   const glm::vec3& rayDir,
                   glm::vec3& outIntersection);

// NEW: Raycast against each external cube in the simulation.
bool RaycastBlocks(const glm::vec3& rayOrigin,
                   const glm::vec3& rayDir,
                   const SimulationBase& sim,
                   float maxDistance,
                   glm::vec3& outIntersection,
                   glm::vec3& outNormal);

#endif // RAYCASTER_HPP