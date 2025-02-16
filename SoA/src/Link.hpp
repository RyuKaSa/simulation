#ifndef LINK_HPP
#define LINK_HPP

#include <glm/glm.hpp>
#include <vector>
#include "PMat.hpp"

// Forward declaration of SoA struct:
struct ParticleSoA;

// This Link class is used by Simulation to apply an external force
// (like "gravity") to all STRUCTURE particles in the SoA.
class Link {
public:
    // Rewritten constructor to accept a reference to ParticleSoA instead of PMat* vector.
    Link(ParticleSoA& soA, const glm::vec3& force);
    void applyGravity();

private:
    ParticleSoA& soA;    // reference to the SoA storing all particles
    glm::vec3 forceValue;
};

#endif // LINK_HPP