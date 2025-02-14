#include "Spring.hpp"
#include <glm/glm.hpp>

Spring::Spring(PMat* p1, PMat* p2, float springConstant, float restLengthScale)
    : p1(p1), p2(p2), k(springConstant), z(0.5f)
{
    // restLength = glm::length(p2->getPosition() - p1->getPosition()) * 0.5f;
    // restLength = glm::distance(p1->getPosition(), p2->getPosition());
    float initialDistance = glm::distance(p1->getPosition(), p2->getPosition());
    restLength = initialDistance * restLengthScale;
}

void Spring::setSpringConstant(float newK) {
    k = newK;
}

void Spring::setDampingCoefficient(float newZ) {
    z = newZ;
}

void Spring::update() {
    // // distance courante
    // float d = glm::distance(p1->getPosition(), p2->getPosition());
    // // direction M1 M2
    // glm::vec3 u = (p1->getPosition() - p2->getPosition()) / d;
    // // force combinées
    // glm::vec3 F = -k * (d - restLength) * u - z * (p2->getVelocity() - p1->getVelocity());
    // // distribution sur M1
    // p1->applyForce(F);
    // // distribution sur M2
    // p2->applyForce(-F);

    float d = glm::distance(p1->getPosition(), p2->getPosition());
    glm::vec3 u = (p1->getPosition() - p2->getPosition()) / d;
    glm::vec3 F = -k * (d - restLength) * u + z * (p2->getVelocity() - p1->getVelocity());
    p1->applyForce(F);
    p2->applyForce(-F);
}

void Spring::Conditional_Update() {
    float d = glm::distance(p1->getPosition(), p2->getPosition());
    float maxLength = 1.1f * restLength;
    glm::vec3 u = (p1->getPosition() - p2->getPosition()) / d;
    glm::vec3 dampingForce = z * (p2->getVelocity() - p1->getVelocity());

    glm::vec3 totalForce;
    if (d > maxLength) {
        float extraStretch = d - maxLength;
        glm::vec3 normalForce = -k * (maxLength - restLength) * u;
        float k_extra = k * 100.0f;
        glm::vec3 extraForce = -k_extra * extraStretch * u;
        totalForce = normalForce + extraForce + dampingForce;
    } else {
        totalForce = -k * (d - restLength) * u + dampingForce;
    }

    // Apply force atomically (Now correctly per instance)
    p1->applyForceThreadSafe(totalForce);
    p2->applyForceThreadSafe(-totalForce);
}

/*
static void update_Damped_Hook(Link *L)
{
    float d = distance(L->M1->pos,L->M2->pos); // distance courante
    Vect u = Vecteur(L->M1->pos,L->M2->pos)/d; // direction M1 M2
    Vect F = -L->k*(d-L->l0)*u -L->z*(L->M2->vit-L->M1->vit); // force combinées
    L->M1->frc += F; // distribution sur M1
    L->M2->frc -= F; // distribution sur M2
}
*/