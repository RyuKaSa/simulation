#ifndef CUBE_PARTICLE_HPP
#define CUBE_PARTICLE_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include "PMat.hpp"
#include <glm/glm.hpp>

class BallParticle : public PMat {
public:
    // Constructor: mass, position, velocity, dimensions, and type (default to EXTERNAL)
    BallParticle(float mass, 
                 const glm::vec3& position, 
                 const glm::vec3& velocity,
                 const glm::vec3& dimensions, 
                 ParticleType type = ParticleType::EXTERNAL);
    
    virtual ~BallParticle() {}

    // Getter for ball dimensions
    const glm::vec3& getDimensions() const;

    // Setter for ball dimensions
    void setDimensions(const glm::vec3& dimensions);

    // (Optional) A basic spawn function that gives a small initial velocity from gravity.
    static BallParticle* spawnBall(const glm::vec3& spawnPosition,
                                   const glm::vec3& dimensions,
                                   float mass,
                                   const glm::vec3& gravity);

private:
    glm::vec3 dims; // dimensions of the ball (width, height, depth)
};

#endif // CUBE_PARTICLE_HPP