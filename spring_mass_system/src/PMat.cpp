#include "PMat.hpp"

static std::atomic<unsigned int> globalParticleId{0};

PMat::PMat(float mass, const glm::vec3& position, ParticleType type, const glm::vec3& velocity)
    : mass(mass), pos(position), vel(velocity), forceAccum(0.0f), type(type)
{
    id = globalParticleId.fetch_add(1);
}

void PMat::applyForce(const glm::vec3& force) {
    forceAccum += force;
}

void PMat::applyForceThreadSafe(const glm::vec3& force) {
    std::scoped_lock lock(mtx);
    forceAccum += force;
}

void PMat::update(float dt) {
    glm::vec3 appliedForce;
    {
        std::scoped_lock lock(mtx);
        appliedForce = forceAccum;
    }
    glm::vec3 acceleration = appliedForce / mass;
    vel += acceleration * dt;
    pos += vel * dt;
    resetForce();

    // Append the current velocity to the history.
    velocityHistory.push_back(vel);
    if (velocityHistory.size() > 100) {
        velocityHistory.pop_front();
    }
}

void PMat::update_fixed(float dt) {
    // position et vitesse restent inchangées
    resetForce();
}

void PMat::resetForce() {
    std::scoped_lock lock(mtx);
    forceAccum = glm::vec3(0.0f);
}

const glm::vec3& PMat::getPosition() const {
    return pos;
}

const glm::vec3& PMat::getVelocity() const {
    std::scoped_lock lock(mtx);
    return vel;
}

float PMat::getMass() const {
    return mass;
}

void PMat::addCorrection(const glm::vec3& correction) {
    std::scoped_lock lock(mtx);
    pos += correction;
}

void PMat::reflectVelocity(const glm::vec3& newVel) {
    std::scoped_lock lock(mtx);
    vel = newVel;
}

/*
#include <PMat.h>
...
// Les "moteurs" : mise à jour de l’état
// intégrateur Leapfrog
static void update_leapfrog(PMat *M, float h)
{
    M->vit += h*M->frc/M->m;// intégration 1 : vitesse m.F(n) = (V(n+1)-V(n))/h -EXplicite
    M->pos += h*M->vit; // intégration 2 : position V(n+1) = (X(n+1)-X(n))/h -IMplicite
    M->frc = 0.; // on vide le buffer de force
}
// mise à jour point fixe : ne fait rien
static void update_fixe(PMat *M, float h)
{
    // position et vitesse restent inchangées
    M->frc = 0.; // on vide le buffer de force (par sécurité)
}
...
extern void M_builder(PMat *M, int type, float m, Point P0, Vect V0)// {
    M->m = m; // masse
    M->pos = P0; // position initiale
    M->vit = V0; // vitesse initiale
    M->frc = 0; // JAMAIS de force à la création
    switch (type)// choix de la fonction de mise à jour
    {
        case 1 : M->update = update_leapfrog; break;// Particule Leapfrog
    }
}
*/