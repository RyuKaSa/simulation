#include "Link.hpp"

// Forward-declared struct from the SoA-based Simulation
#include <iostream>
#include <mutex>

// SoA that must match what's in Simulation
struct ParticleSoA {
    std::vector<glm::vec3> position;
    std::vector<glm::vec3> velocity;
    std::vector<glm::vec3> forceAccum;
    std::vector<float>      mass;
    std::vector<ParticleType> type;
    std::vector<bool>       isStatic;
    // optional: color, dims...
};

Link::Link(ParticleSoA& soAref, const glm::vec3& force)
    : soA(soAref), forceValue(force)
{
}

void Link::applyGravity() {
    // Apply 'forceValue' to all STRUCTURE-type particles in soA.
    size_t n = soA.position.size();
    for (size_t i = 0; i < n; i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            // Add force to forceAccum
            soA.forceAccum[i] += forceValue;
        }
    }
}