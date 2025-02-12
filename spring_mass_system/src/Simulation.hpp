#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>
#include "PMat.hpp"
#include "Spring.hpp"
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
private:
    float springConstant;
    std::vector<Ball> balls;
    PMat* ballLeft;
    PMat* ballCenter;
    PMat* ballRight;
    Spring* springLeftCenter;
    Spring* springCenterRight;
};

#endif // SIMULATION_H