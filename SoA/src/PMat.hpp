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

// Now using double instead of float
class PMat {
public:
    PMat(double mass,
         const glm::dvec3& position,
         ParticleType type = ParticleType::STRUCTURE,
         const glm::dvec3& velocity = glm::dvec3(0.0));

    // Apply an external force
    void applyForce(const glm::dvec3& force);

    void applyForceThreadSafe(const glm::dvec3& force);

    // Update position and velocity
    void update(double dt);

    // Update position and velocity without changing them
    void update_fixed(double dt);

    // Getters
    const glm::dvec3& getPosition() const;
    const glm::dvec3& getVelocity() const;
    double getMass() const;

    void addCorrection(const glm::dvec3& correction);
    void reflectVelocity(const glm::dvec3& newVel);

    // Reset accumulated force (should be called after update)
    void resetForce();

    virtual ~PMat() {}

    mutable std::mutex mtx;

    unsigned int id;
    ParticleType type;

private:
    double mass;
    glm::dvec3 pos;
    glm::dvec3 vel;
    glm::dvec3 forceAccum;
    // If you truly need atomic force accum, you'd need a workaround, because
    // there's no built-in atomic for dvec3. We'll remove or keep as stub:
    std::atomic<double> dummyAtomic; // example placeholder

    std::deque<glm::dvec3> velocityHistory;
};

#endif // PMAT_HPP