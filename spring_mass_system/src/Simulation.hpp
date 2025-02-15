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
#include <thread>
#include <algorithm>
#include "BallParticle.hpp"
#include <array>
#include <numeric>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/compatibility.hpp>
struct Ball {
    glm::vec3 position;
    glm::vec3 color;
    ParticleType type;
    glm::vec3 dimensions;
};

struct HexFace {
    std::array<glm::vec3, 3> triangle; // Store as triangles for collision
    glm::vec3 normal;
    int hexagonIndex;
};

class Simulation {
public:
    Simulation();
    ~Simulation();

    void Initialization();

    void reset();

    void update(float dt);
    void updateHexFaces();
    void collisionDetectionAndResolution(float dt);
    void setSpringConstant(float k);
    void setDampingCoefficient(float z);

    const std::vector<Ball>& getBalls() const;
    std::vector<glm::vec3> getParticlePositions() const;
    std::vector<glm::vec3> getStructureParticlePositions() const;
    const std::vector<glm::vec3> getParticleVelocities() const;
    const std::vector<glm::vec3> getStructureParticleVelocities() const;
    std::vector<glm::vec3> getSpringEndpoints() const;

    // getter for impulseScaling
    float getImpulseScaling() const { return impulseScaling; }
    void setImpulseScaling(float scaling) { impulseScaling = scaling; }

    std::vector<glm::vec3> getHexHitboxTriangles() const;

    void clearSimulation();  
    void createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic);
    void createHexGrid(int hexCount, float hexagonSize, float springRestLength, bool bothEndsStatic, float orientationDegrees);
    void createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength);
    void applyGravityLink();

    void throwBall(const glm::vec3& cameraPos, const glm::vec3& cameraDir,
                float speed = 10.0f, float mass = 1.0f,
                const glm::vec3& dimensions = glm::vec3(0.5f));

    bool isPointInTriangle(const glm::vec3& point, 
                const std::array<glm::vec3, 3>& triangle,
                const glm::vec3& normal) const;
private:
    float springConstant;
    Link* gravityLink;
    std::vector<Ball> balls;

    std::vector<PMat*> ballObjects;
    std::vector<Spring*> springs;
    bool bothEndsStatic;
    std::vector<bool> ballStaticFlags;

    std::vector<HexFace> hexFaces;
    std::vector<std::vector<glm::vec3>> hexagonVertexLists;
    std::vector<std::vector<int>> hexagonIndices;

    mutable std::recursive_mutex simulationMutex;
    float impulseScaling = 1000.0f;
};

#endif // SIMULATION_H