#include "BlockPlacement.hpp"
#include "Raycaster.hpp"
#include <glm/glm.hpp>

// For placing a new block so it touches the face of the block we clicked
static const float FACE_OFFSET = 0.50f; // For adding blocks

bool GetBlockAdditionCoord(const glm::vec3& rayOrigin,
                           const glm::vec3& rayDir,
                           const BlockWorld &world,
                           const SimulationBase &sim,
                           GridCoord &outCoord)
{
    const float maxDistance = 15.0f; // adjust as needed

    // 1) Raycast against existing blocks:
    glm::vec3 hitPoint, hitNormal;
    if (RaycastBlocks(rayOrigin, rayDir, sim, maxDistance, hitPoint, hitNormal))
    {
        // For *addition*, we offset forward by half-block to place the new block flush with the face.
        float halfCell = world.getGridSpacing() * 0.5f;
        glm::vec3 newBlockPos = hitPoint + hitNormal * (halfCell * FACE_OFFSET);

        // Snap to grid
        outCoord = world.worldToGrid(newBlockPos.x, newBlockPos.y, newBlockPos.z);
        if (outCoord.y < 0) outCoord.y = 0;

        return true;
    }
    else
    {
        // 2) Fallback to ground intersection if no block was hit
        glm::vec3 intersection;
        if (!RaycastGround(rayOrigin, rayDir, intersection))
            return false;

        if (glm::distance(rayOrigin, intersection) > maxDistance)
            return false;

        // Snap to grid:
        outCoord = world.worldToGrid(intersection.x, intersection.y, intersection.z);
        if (outCoord.y < 0) outCoord.y = 0;

        return true;
    }
}

bool GetBlockRemovalCoord(const glm::vec3& rayOrigin,
                          const glm::vec3& rayDir,
                          const BlockWorld &world,
                          const SimulationBase &sim,
                          GridCoord &outCoord)
{
    const float maxDistance = 15.0f; // same limit

    glm::vec3 hitPoint, hitNormal;
    if (!RaycastBlocks(rayOrigin, rayDir, sim, maxDistance, hitPoint, hitNormal))
        return false; // did not hit any existing block at all

    // For *removal*, we shift the intersection point slightly *inside* the block
    // so that worldToGrid(...) picks the correct block that was clicked.
    glm::vec3 adjustedPoint = hitPoint - hitNormal * 0.01f;

    outCoord = world.worldToGrid(adjustedPoint.x, adjustedPoint.y, adjustedPoint.z);
    // If we want to clamp it so we don't get negative Y:
    if (outCoord.y < 0) 
        outCoord.y = 0;

    return true;
}