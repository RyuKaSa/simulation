#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include "PMat.hpp"       // needed for ParticleType
#include "Link.hpp"
#include "SimulationSoAInternals.hpp"
#include <set>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>

// Data used by the renderer
struct Ball {
    glm::vec3 position;
    glm::vec3 color;
    ParticleType type;
    glm::vec3 dimensions;
};

// SoA for rendering
struct BallSoA {
    std::vector<float> posX, posY, posZ;
    std::vector<float> colorR, colorG, colorB;
    std::vector<int>   types;     // e.g. 0 = STRUCTURE, 1 = EXTERNAL
    std::vector<float> dimsX, dimsY, dimsZ;

    void resize(size_t n) {
        posX.resize(n);
        posY.resize(n);
        posZ.resize(n);
        colorR.resize(n);
        colorG.resize(n);
        colorB.resize(n);
        types.resize(n);
        dimsX.resize(n);
        dimsY.resize(n);
        dimsZ.resize(n);
    }
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
    float getImpulseScaling() const { return impulseScaling; }
    void setImpulseScaling(float scaling) { impulseScaling = scaling; }

    // create structures
    void createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic);
    void createHexGrid(int hexCount, float hexagonSize, float springRestLength, 
                       bool bothEndsStatic, float orientationDegrees);
    void createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength);

    // gravity link usage
    void applyGravityLink();

    // rendering queries
    const std::vector<Ball>& getBalls() const;
    std::vector<glm::vec3> getParticlePositions() const;
    std::vector<glm::vec3> getStructureParticlePositions() const;
    const std::vector<glm::vec3> getParticleVelocities() const;
    const std::vector<glm::vec3> getStructureParticleVelocities() const;
    std::vector<glm::vec3> getSpringEndpoints() const;

    void clearSimulation();

    const std::vector<Ball>& getSnapshotBalls() const;
    BallSoA getSnapshotSoA() const;
    BallSoA convertBallsToSoA(const std::vector<Ball>& balls) const;

    // Async update methods (added to match definitions in Simulation.cpp)
    void startAsyncUpdates();
    void stopAsyncUpdates();

    // concurrency info
    std::atomic<int> effectiveStepsPerSecond { 0 };
    std::atomic<float> lastPhysicsUpdateTime { 0.0f };

private:
    void asyncLoop();

    // SoA for all particles
    ParticleSoA soA;
    // Vector of all springs
    std::vector<SpringData> springs;

    // Gravity link that references SoA
    Link* gravityLink = nullptr;

    // For legacy “Ball” usage in snapshots
    std::vector<Ball> balls;

    // settings
    float springConstant = 0.5f;
    bool bothEndsStatic  = false;
    float impulseScaling = 1000.0f;

    // concurrency
    mutable std::recursive_mutex simulationMutex;
    mutable std::mutex snapshotMutex;
    std::atomic<bool> asyncRunning { false };
    std::thread asyncThread;

    std::vector<Ball> snapshotBalls;
    BallSoA snapshotSoA;
};

#endif