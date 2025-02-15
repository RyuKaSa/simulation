#ifndef CUBE_PARTICLE_HPP
#define CUBE_PARTICLE_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include "PMat.hpp"
#include <glm/glm.hpp>

class CubeParticle : public PMat {
public:
    // Constructor: mass, position, velocity, dimensions, and type (default to EXTERNAL)
    CubeParticle(float mass, 
                 const glm::vec3& position, 
                 const glm::vec3& velocity,
                 const glm::vec3& dimensions, 
                 ParticleType type = ParticleType::EXTERNAL);
    
    virtual ~CubeParticle() {}

    // Getter for cube dimensions
    const glm::vec3& getDimensions() const;

    // Setter for cube dimensions
    void setDimensions(const glm::vec3& dimensions);

    // (Optional) A basic spawn function that gives a small initial velocity from gravity.
    static CubeParticle* spawnCube(const glm::vec3& spawnPosition,
                                   const glm::vec3& dimensions,
                                   float mass,
                                   const glm::vec3& gravity);

private:
    glm::vec3 dims; // dimensions of the cube (width, height, depth)
};

#endif // CUBE_PARTICLE_HPP