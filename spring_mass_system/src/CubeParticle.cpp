#include "CubeParticle.hpp"
#include <glm/gtx/transform.hpp>

CubeParticle::CubeParticle(float mass, 
                           const glm::vec3& position, 
                           const glm::vec3& velocity,
                           const glm::vec3& dimensions, 
                           ParticleType type)
    : PMat(mass, position, type, velocity), dims(dimensions)
{
}

const glm::vec3& CubeParticle::getDimensions() const {
    return dims;
}

void CubeParticle::setDimensions(const glm::vec3& dimensions) {
    dims = dimensions;
}

CubeParticle* CubeParticle::spawnCube(const glm::vec3& spawnPosition,
                                      const glm::vec3& dimensions,
                                      float mass,
                                      const glm::vec3& gravity) {
    // Give the cube a small initial velocity in the direction of gravity.
    glm::vec3 initialVelocity = gravity * 0.1f;
    return new CubeParticle(mass, spawnPosition, initialVelocity, dimensions, ParticleType::EXTERNAL);
}