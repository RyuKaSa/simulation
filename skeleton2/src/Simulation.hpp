#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>

struct Ball {
    glm::vec3 position;
    glm::vec3 color;
};

class Simulation {
public:
    Simulation();
    void update(float dt);
    void setSpringConstant(float k);
    const std::vector<Ball>& getBalls() const;
private:
    float springConstant;
    std::vector<Ball> balls;
};

#endif // SIMULATION_H