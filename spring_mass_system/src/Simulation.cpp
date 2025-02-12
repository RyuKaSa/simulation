#include "Simulation.hpp"

Simulation::Simulation() : springConstant(0.5f) {
    // Create three PMats.
    // Left and right balls are static (we won't call update on them).
    // ballLeft   = new PMat(1.0f, glm::vec3(-0.5f, 0.0f, 0.0f));
    // ballCenter = new PMat(1.0f, glm::vec3( 0.5f, 0.5f, 0.5f));
    // ballRight  = new PMat(1.0f, glm::vec3( 0.5f, 0.0f, 0.0f));

    createCord(8, 7.0f, 0.1f, true); // Default cord setup (3 balls, 1m length, both ends static)

    gravityLink = new Link(ballObjects, glm::vec3(0.0f, -9.81f, 0.0f));  // ✅ Use all particles
}

Simulation::~Simulation() {
    clearSimulation();
    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

void Simulation::setSpringConstant(float k) {
    springConstant = k;
    for (Spring* spring : springs) {
        spring->setSpringConstant(k);
    }
}

void Simulation::setDampingCoefficient(float z) {
    for (Spring* spring : springs) {
        spring->setDampingCoefficient(z);
    }
}

void Simulation::update(float dt) {
    for (Spring* spring : springs) {
        spring->update();
    }

    if (gravityLink) {
        applyGravityLink();
    }

    for (size_t i = 0; i < ballObjects.size(); i++) {
        bool isStatic = bothEndsStatic ? (i == 0 || i == ballObjects.size() - 1) : (i == 0);

        if (isStatic) {
            ballObjects[i]->update_fixed(dt);
        } else {
            ballObjects[i]->update(dt);
        }
    }

    for (size_t i = 0; i < ballObjects.size(); i++) {
        balls[i].position = ballObjects[i]->getPosition();
    }
}

const std::vector<Ball>& Simulation::getBalls() const {
    return balls;
}

void Simulation::createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic) {
    clearSimulation(); // Remove any existing simulation state

    float spacing = length / (numBalls - 1);
    glm::vec3 startPos(-length / 2, 0.0f, 0.0f);

    for (int i = 0; i < numBalls; i++) {
        bool isStatic = bothEndsStatic ? (i == 0 || i == numBalls - 1) : (i == 0);
        
        PMat* ball = new PMat(1.0f, startPos + glm::vec3(i * spacing, 0.0f, 0.0f));
        ballObjects.push_back(ball);

        Ball b;
        b.position = ball->getPosition();
        b.color = glm::vec3(1.0f, 0.0f, 0.0f); // Default red color
        balls.push_back(b);
    }

    for (int i = 0; i < numBalls - 1; i++) {
        Spring* spring = new Spring(ballObjects[i], ballObjects[i + 1], springConstant);
        spring->setDampingCoefficient(0.5f);
        springs.push_back(spring);
    }

    if (ballObjects.size() >= 2) {
        delete gravityLink;
        gravityLink = new Link(ballObjects, glm::vec3(0.0f, -9.81f, 0.0f));
    }
}

void Simulation::applyGravityLink() {
    // Apply gravity to all particles in the system using the gravity link
    gravityLink->applyGravity();
}

std::vector<glm::vec3> Simulation::getParticlePositions() const {
    std::vector<glm::vec3> positions;
    for (const auto* ball : ballObjects) {
        positions.push_back(ball->getPosition());
    }
    return positions;
}

const std::vector<glm::vec3> Simulation::getParticleVelocities() const {
    std::vector<glm::vec3> velocities;
    for (const auto& ball : ballObjects) {
        velocities.push_back(ball->getVelocity());
    }
    return velocities;
}

void Simulation::clearSimulation() {
    for (Spring* spring : springs) {
        delete spring;
    }
    springs.clear();

    for (PMat* ball : ballObjects) {
        delete ball;
    }
    ballObjects.clear();

    balls.clear(); // Clear visual representation

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}