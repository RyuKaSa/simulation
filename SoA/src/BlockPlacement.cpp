#include "BlockPlacement.hpp"
#include "Raycaster.hpp"
#include <glm/glm.hpp>

// If you want to place a new block so it touches the face of the block you clicked,
// define an offset factor:
static const float FACE_OFFSET = 0.50f; // Slightly more than half block thickness

bool GetBlockPlacementPosition(const glm::vec3& rayOrigin,
                               const glm::vec3& rayDir,
                               const BlockWorld &world,
                               const SimulationBase &sim,
                               GridCoord &outCoord)
{
    // Maximum allowed ray distance:
    const float maxDistance = 15.0f; // adjust as needed

    // 1) Try to raycast against existing blocks:
    glm::vec3 hitPoint, hitNormal;
    if (RaycastBlocks(rayOrigin, rayDir, sim, maxDistance, hitPoint, hitNormal))
    {
        // When a block is hit, offset the intersection point by the face normal
        // to compute a new block position.
        float halfCell = world.getGridSpacing() * 0.5f;
        glm::vec3 newBlockPos = hitPoint + hitNormal * (halfCell * FACE_OFFSET);

        // Snap newBlockPos to the grid:
        outCoord = world.worldToGrid(newBlockPos.x, newBlockPos.y, newBlockPos.z);
        if (outCoord.y < 0)
            outCoord.y = 0;
        return true;
    }
    else
    {
        // 2) Fallback to ground intersection if no block was hit
        glm::vec3 intersection;
        if (!RaycastGround(rayOrigin, rayDir, intersection))
            return false;

        // Check distance to ensure we’re not placing blocks infinitely far:
        if (glm::distance(rayOrigin, intersection) > maxDistance)
            return false;

        // Snap the intersection point to the grid:
        outCoord = world.worldToGrid(intersection.x, intersection.y, intersection.z);
        if (outCoord.y < 0)
            outCoord.y = 0;
        return true;
    }
}