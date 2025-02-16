#ifndef SIMULATION_SOA_INTERNALS_HPP
#define SIMULATION_SOA_INTERNALS_HPP

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp" // for ParticleType

// The main SoA struct
struct ParticleSoA {
    std::vector<glm::vec3> position;
    std::vector<glm::vec3> velocity;
    std::vector<glm::vec3> forceAccum; 
    std::vector<float> mass;
    std::vector<ParticleType> type;
    std::vector<bool> isStatic;

    std::vector<glm::vec3> color;
    std::vector<glm::vec3> dimensions;
};

struct SpringData {
    int p1Index;
    int p2Index;
    float restLength;
    float springConstant;
    float damping;
};

#endif // SIMULATION_SOA_INTERNALS_HPP