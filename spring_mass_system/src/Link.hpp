#ifndef LINK_HPP
#define LINK_HPP

#include <vector>
#include "PMat.hpp"

class Link {
public:
    Link(std::vector<PMat*>& particles, glm::vec3 force);
    void applyGravity();  // Apply gravity to all particles in the link

private:
    std::vector<PMat*>& particleList;  // ✅ Store reference to all particles
    glm::vec3 forceValue;  // External force (gravity)
};

#endif // LINK_HPP