#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp"
#include "Spring.hpp"
#include "Link.hpp"
#include <iostream>
#include <set>
#include <utility>

struct Ball {
    glm::vec3 position;
    glm::vec3 color;
};

class Simulation {
public:
    Simulation();
    ~Simulation();

    void Initialization();

    void reset();

    void update(float dt);
    void setSpringConstant(float k);
    void setDampingCoefficient(float z);

    const std::vector<Ball>& getBalls() const;
    std::vector<glm::vec3> getParticlePositions() const;
    const std::vector<glm::vec3> getParticleVelocities() const;
    std::vector<glm::vec3> getSpringEndpoints() const;

    void clearSimulation();  
    void createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic);
    void createHexGrid(int hexCount, float hexagonSize, float springRestLength);
    void createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength);
    void applyGravityLink();
private:
    float springConstant;
    Link* gravityLink;
    std::vector<Ball> balls;

    std::vector<PMat*> ballObjects;
    std::vector<Spring*> springs;
    bool bothEndsStatic;
    std::vector<bool> ballStaticFlags;
};

#endif // SIMULATION_H