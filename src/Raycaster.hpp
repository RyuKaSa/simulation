#ifndef RAYCASTER_HPP
#define RAYCASTER_HPP

#include <glm/glm.hpp>
#include "Simulation.hpp"

bool RaycastGround(const glm::vec3& rayOrigin,
                   const glm::vec3& rayDir,
                   glm::vec3& outIntersection);

bool RaycastBlocks(const glm::vec3& rayOrigin,
                   const glm::vec3& rayDir,
                   const SimulationBase& sim,
                   float maxDistance,
                   glm::vec3& outIntersection,
                   glm::vec3& outNormal);

#endif // RAYCASTER_HPP