#include "Link.hpp"
#include <iostream>
#include <mutex>

Link::Link(ParticleSoA& soAref, const glm::dvec3& force)
    : soA(soAref), forceValue(force)
{
}

void Link::applyGravity() {
    // Apply forceValue to all structure-type particles
    size_t n = soA.position.size();
    for (size_t i = 0; i < n; i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            soA.forceAccum[i] += forceValue;
        }
    }
}