#ifndef PMAT_HPP
#define PMAT_HPP

#include <glm/glm.hpp>
#include <mutex>
#include <atomic>
#include <deque>

enum class ParticleType {
    STRUCTURE,
    EXTERNAL
};

class PMat {
public:
    PMat(float mass, const glm::vec3& position, ParticleType type = ParticleType::STRUCTURE, const glm::vec3& velocity = glm::vec3(0.0f));

    // Apply an external force
    void applyForce(const glm::vec3& force);

    void applyForceThreadSafe(const glm::vec3& force);
    
    // Update position and velocity
    void update(float dt);

    // Update position and velocity without changing them
    void update_fixed(float dt);
    
    // Getters
    const glm::vec3& getPosition() const;
    const glm::vec3& getVelocity() const;
    // get mass
    float getMass() const;

    void addCorrection(const glm::vec3& correction);
    void reflectVelocity(const glm::vec3& newVel);

    // Reset accumulated force (should be called after update)
    void resetForce();

    virtual ~PMat() {}

    mutable std::mutex mtx;

    unsigned int id;             // a unique id assigned at creation
    ParticleType type;           // distinguishes structure vs. external
    
private:
    float mass;
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 forceAccum;
    std::atomic<glm::vec3> forceAccumAtomic;

    std::deque<glm::vec3> velocityHistory;
};

#endif // PMAT_HPP