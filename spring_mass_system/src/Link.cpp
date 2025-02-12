#include "Link.hpp"
#include <iostream>

Link::Link(std::vector<PMat*>& particles, glm::vec3 force)
    : particleList(particles), forceValue(force) {}

void Link::applyGravity() {
    if (particleList.empty()) {
        std::cerr << "Error: No particles in Link!" << std::endl;
        return;
    }

    // std::cout << "Applying gravity to " << particleList.size() << " particles..." << std::endl;

    for (PMat* particle : particleList) {
        if (particle) {
            particle->applyForce(forceValue);
        }
    }
}