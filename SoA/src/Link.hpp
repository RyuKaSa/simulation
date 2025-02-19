#ifndef LINK_HPP
#define LINK_HPP

#include <glm/glm.hpp>
#include <vector>
#include "PMat.hpp"

// Forward declaration of SoA struct:
struct ParticleSoA;

// Applies e.g. gravity (a dvec3 force) to all structure particles:
class Link {
public:
    Link(ParticleSoA& soA, const glm::dvec3& force);
    void applyGravity();

private:
    ParticleSoA& soA;
    glm::dvec3 forceValue;
};

#endif // LINK_HPP