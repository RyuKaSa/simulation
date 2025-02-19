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

struct HexTriangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
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

    // Create structures
    void addStaticCubeUnderGrid();
    void createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic);
    void createHexGrid(int hexCount, float hexagonSize, float springRestLength, 
                       bool bothEndsStatic, float orientationDegrees);
    void createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength);
    void createMultiLayerHexGrid(int numHexagons, float hexagonSize, float springRestLength,
                                        bool bothEndsStatic, float orientationDegrees, float layerHeight, int nLayers);

    // Gravity link usage
    void applyGravityLink();

    // Expose the simulation’s particle SoA directly.
    const ParticleSoA& getSoA() const;

    // Convenience getters for rendering:
    const std::vector<HexTriangle>& getHexTriangles() const;
    std::vector<glm::vec3> getStructureParticlePositions() const;

    // Clear the simulation (for reset/reinitialization)
    void clearSimulation();

    // Async update methods
    void startAsyncUpdates();
    void stopAsyncUpdates();

    // Update hexagon triangles
    void updateHexTriangles();

    // Concurrency info
    std::atomic<int> effectiveStepsPerSecond { 0 };
    std::atomic<float> lastPhysicsUpdateTime { 0.0f };

    size_t getSpringCount() const;
    const std::vector<SpringData>& getSprings() const;

private:
    void asyncLoop();

    // SoA for all particles
    ParticleSoA soA;
    // Vector of all springs
    std::vector<SpringData> springs;

    // Gravity link that references the SoA
    Link* gravityLink = nullptr;

    // Settings
    float springConstant = 0.5f;
    bool bothEndsStatic  = false;
    float impulseScaling = 1000.0f;

    // Concurrency
    mutable std::recursive_mutex simulationMutex;
    std::atomic<bool> asyncRunning { false };
    std::thread asyncThread;

    // Reusable thread pool for parallel work
    class ThreadPool; // Forward declaration; definition in Simulation.cpp.
    ThreadPool* threadPool = nullptr;

    // New members to support hexagon triangles
    std::vector<std::vector<glm::vec3>> hexagonVertexLists;
    std::vector<std::vector<int>> hexagonIndices;
    std::vector<HexTriangle> hexTriangles;

    glm::mat4 createRotationMatrix(float orientationDegrees);
    void generateHexagonCells(int numHexagons, float hexagonSize, const glm::mat4& rotationMatrix,
                              std::vector<glm::vec3>& uniquePositions, 
                              std::set<std::pair<int,int>>& edgeSet);
    void assignUniquePositionsToSoA(const std::vector<glm::vec3>& uniquePositions, bool bothEndsStatic);
    void createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, float springRestLength);
};

#endif