#include "Simulation.hpp"

Simulation::Simulation() : springConstant(0.5f) {
    // Initialize two balls with fixed positions and colors.
    Ball ball1;
    ball1.position = glm::vec3(-0.3f, 0.0f, 0.0f);
    ball1.color = glm::vec3(1.0f, 0.0f, 0.0f); // red

    Ball ball2;
    ball2.position = glm::vec3(0.3f, 0.0f, 0.0f);
    ball2.color = glm::vec3(0.0f, 0.0f, 1.0f); // blue

    balls.push_back(ball1);
    balls.push_back(ball2);
}

void Simulation::update(float dt) {
    // Update simulation here.
    // Currently the balls are static; springConstant can later affect dynamics.
}

void Simulation::setSpringConstant(float k) {
    springConstant = k;
}

const std::vector<Ball>& Simulation::getBalls() const {
    return balls;
}