#include "Raycaster.hpp"
#include <cmath>
#include <limits>

bool RaycastGround(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, glm::vec3 &outIntersection)
{
    if (std::abs(rayDir.y) < 1e-6f)
        return false; // Ray is parallel to ground

    float t = -rayOrigin.y / rayDir.y;
    if (t < 0)
        return false; // Intersection behind origin

    outIntersection = rayOrigin + t * rayDir;
    return true;
}

// Helper: Ray-AABB intersection for a single block.
// Returns distance if hit, else returns -1.f if no intersection.
static float IntersectAABB(const glm::vec3 &origin, const glm::vec3 &dir,
                           const glm::vec3 &boxMin, const glm::vec3 &boxMax,
                           glm::vec3 &outNormal)
{
    // Based on the “slab” method for AABB intersection.
    // tmin/tmax track the param range along the ray.
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();
    outNormal = glm::vec3(0.0f);

    // Check each axis
    for (int i = 0; i < 3; i++)
    {
        if (std::abs(dir[i]) < 1e-6f)
        {
            // Ray is parallel on this axis: if origin not in [min..max], no hit
            if (origin[i] < boxMin[i] || origin[i] > boxMax[i])
            {
                return -1.f;
            }
        }
        else
        {
            float o = origin[i];
            float d = dir[i];
            float invD = 1.f / d;
            float t1 = (boxMin[i] - o) * invD;
            float t2 = (boxMax[i] - o) * invD;
            float nearT = glm::min(t1, t2);
            float farT = glm::max(t1, t2);

            // If nearT is bigger than overall tmax or farT < overall tmin => no hit
            if (nearT > tmax || farT < tmin)
            {
                return -1.f;
            }

            if (nearT > tmin)
            {
                tmin = nearT;
                // We can guess a normal for the near plane
                // sign depends on the sign of d
                glm::vec3 normal(0.f);
                normal[i] = (t1 > t2) ? 1.f : -1.f;
                outNormal = normal;
            }
            if (farT < tmax)
            {
                tmax = farT;
            }
        }
    }

    if (tmax < 0.f)
    {
        return -1.f;
    }
    // We take tmin as the valid intersection param if it’s positive
    // (meaning the intersection is in front).
    if (tmin < 0.f)
    {
        // If tmin < 0, maybe tmax is the first intersection? (Ray starts inside the box.)
        if (tmax > 0.f)
        {
            // This means the origin is inside the box.
            // Out normal is not well-defined, but we can set it to something.
            outNormal = glm::vec3(0.f, 1.f, 0.f);
            return 0.f; // distance = 0
        }
        return -1.f;
    }

    return tmin;
}

// Slight changes to clarify we do no special offset *here*:
bool RaycastBlocks(const glm::vec3 &rayOrigin,
                   const glm::vec3 &rayDir,
                   const SimulationBase &sim,
                   float maxDistance,
                   glm::vec3 &outIntersection,
                   glm::vec3 &outNormal)
{
    // The intersection logic remains basically the same
    ParticleSoA soA = sim.getSoACopy();
    float nearestT = maxDistance;
    bool hitAny = false;

    for (size_t i = 0; i < soA.position.size(); i++)
    {
        if (soA.type[i] != ParticleType::EXTERNAL)
            continue;

        glm::vec3 center((float)soA.position[i].x,
                         (float)soA.position[i].y,
                         (float)soA.position[i].z);
        glm::vec3 half((float)soA.dimensions[i].x * 0.5f);

        glm::vec3 bmin = center - half;
        glm::vec3 bmax = center + half;

        glm::vec3 boxNormal(0.f);
        float t = IntersectAABB(rayOrigin, rayDir, bmin, bmax, boxNormal);
        if (t >= 0.f && t < nearestT)
        {
            nearestT = t;
            outNormal = boxNormal;
            hitAny = true;
        }
    }

    if (!hitAny)
        return false;

    // The EXACT intersection is rayOrigin + rayDir * t
    if (nearestT < 0.f || nearestT > maxDistance)
        return false;

    outIntersection = rayOrigin + rayDir * nearestT;
    return true;
}