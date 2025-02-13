#ifndef PMAT_HPP
#define PMAT_HPP

#include <glm/glm.hpp>
#include <mutex>

class PMat {
public:
    PMat(float mass, const glm::vec3& position, const glm::vec3& velocity = glm::vec3(0.0f));
    
    // Apply an external force
    void applyForce(const glm::vec3& force);
    
    // Update position and velocity
    void update(float dt);

    // Update position and velocity without changing them
    void update_fixed(float dt);
    
    // Getters
    const glm::vec3& getPosition() const;
    const glm::vec3& getVelocity() const;

    // Reset accumulated force (should be called after update)
    void resetForce();

    std::mutex mtx;
    
private:
    float mass;
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 forceAccum;
};

#endif // PMAT_HPP