#include "Link.hpp"
#include <iostream>

Link::Link(std::vector<PMat*>& particles, glm::vec3 force)
    : particleList(particles), forceValue(force) {}

void Link::applyGravity() {
    if (particleList.empty()) {
        std::cerr << "Error: No particles in Link!" << std::endl;
        return;
    }

    for (PMat* particle : particleList) {
        // Only apply gravity if the particle is part of the structure.
        if (particle && particle->type == ParticleType::STRUCTURE) {
            particle->applyForceThreadSafe(forceValue);
        }
    }
}