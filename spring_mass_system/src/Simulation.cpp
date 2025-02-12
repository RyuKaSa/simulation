#include "Simulation.hpp"

Simulation::Simulation() : springConstant(0.5f) {
    // Create three PMats.
    // Left and right balls are static (we won't call update on them).
    ballLeft   = new PMat(1.0f, glm::vec3(-0.5f, 0.0f, 0.0f));
    ballCenter = new PMat(1.0f, glm::vec3( 0.5f, 0.5f, 0.5f));
    ballRight  = new PMat(1.0f, glm::vec3( 0.5f, 0.0f, 0.0f));

    // Create springs connecting left-center and center-right.
    springLeftCenter = new Spring(ballLeft, ballCenter, springConstant);
    springCenterRight = new Spring(ballCenter, ballRight, springConstant);

    // Set up balls for rendering.
    Ball bLeft;
    bLeft.position = ballLeft->getPosition();
    bLeft.color = glm::vec3(1.0f, 0.0f, 0.0f);  // red
    balls.push_back(bLeft);

    Ball bCenter;
    bCenter.position = ballCenter->getPosition();
    bCenter.color = glm::vec3(0.0f, 1.0f, 0.0f);  // green
    balls.push_back(bCenter);

    Ball bRight;
    bRight.position = ballRight->getPosition();
    bRight.color = glm::vec3(0.0f, 0.0f, 1.0f);  // blue
    balls.push_back(bRight);
}

Simulation::~Simulation() {
    delete springLeftCenter;
    delete springCenterRight;
    delete ballLeft;
    delete ballCenter;
    delete ballRight;
}

void Simulation::setSpringConstant(float k) {
    springConstant = k;
    springLeftCenter->setSpringConstant(k);
    springCenterRight->setSpringConstant(k);
}

void Simulation::setDampingCoefficient(float z) {
    springLeftCenter->setDampingCoefficient(z);
    springCenterRight->setDampingCoefficient(z);
}

void Simulation::update(float dt) {
    // std::cout << " - update Start: " << std::endl;

    // Compute forces from springs.
    springLeftCenter->update();
    springCenterRight->update();

    // Only update the center (dynamic) ball.
    ballCenter->update(dt);
    ballLeft->update_fixed(dt);
    ballRight->update_fixed(dt);

    // Update rendering positions.
    balls[0].position = ballLeft->getPosition();
    balls[1].position = ballCenter->getPosition();
    balls[2].position = ballRight->getPosition();
}

const std::vector<Ball>& Simulation::getBalls() const {
    return balls;
}