#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp"
#include "Spring.hpp"
#include "Link.hpp"
#include <iostream>

struct Ball {
    glm::vec3 position;
    glm::vec3 color;
};

class Simulation {
public:
    Simulation();
    ~Simulation();
    void update(float dt);
    void setSpringConstant(float k);
    void setDampingCoefficient(float z);
    const std::vector<Ball>& getBalls() const;

    void clearSimulation();  
    void createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic);
    void applyGravityLink();
private:
    float springConstant;
    Link* gravityLink;
    std::vector<Ball> balls;

    std::vector<PMat*> ballObjects;
    std::vector<Spring*> springs;
    bool bothEndsStatic;
};

#endif // SIMULATION_H