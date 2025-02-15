#include "BallParticle.hpp"
#include <glm/gtx/transform.hpp>

BallParticle::BallParticle(float mass, 
                           const glm::vec3& position, 
                           const glm::vec3& velocity,
                           const glm::vec3& dimensions, 
                           ParticleType type)
    : PMat(mass, position, type, velocity), dims(dimensions)
{
}

const glm::vec3& BallParticle::getDimensions() const {
    return dims;
}

void BallParticle::setDimensions(const glm::vec3& dimensions) {
    dims = dimensions;
}

BallParticle* BallParticle::spawnBall(const glm::vec3& spawnPosition,
                                      const glm::vec3& dimensions,
                                      float mass,
                                      const glm::vec3& gravity) {
    // Give the ball a small initial velocity in the direction of gravity.
    glm::vec3 initialVelocity = gravity * 0.1f;
    return new BallParticle(mass, spawnPosition, initialVelocity, dimensions, ParticleType::EXTERNAL);
}