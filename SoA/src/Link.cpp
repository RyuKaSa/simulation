#include "Link.hpp"
#include <iostream>
#include <mutex>

// SoA must match what's in SimulationSoAInternals.hpp but for double-based:
struct ParticleSoA {
    std::vector<glm::dvec3> position;
    std::vector<glm::dvec3> velocity;
    std::vector<glm::dvec3> forceAccum;
    std::vector<double>      mass;
    std::vector<ParticleType> type;
    std::vector<bool>       isStatic;
    // color, dims not needed here
};

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