#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <glm/glm.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include "PMat.hpp"
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
    void update(double dt);

    void setSpringConstant(double k);
    void setDampingCoefficient(double z);
    double getImpulseScaling() const { return impulseScaling; }
    void setImpulseScaling(double scaling) { impulseScaling = scaling; }

    // Create structures
    void addStaticCubeUnderGrid();
    void createCord(int numBalls, double length, double springRestLength, bool bothEndsStatic);
    void createHexGrid(int hexCount, double hexagonSize, double springRestLength,
                       bool bothEndsStatic, double orientationDegrees);
    void createSquareGridWithDiagonals(int gridSize, double spacing, double springRestLength);
    void createMultiLayerHexGrid(int numHexagons, double hexagonSize, double springRestLength,
                                 bool bothEndsStatic, double orientationDegrees, double layerHeight, int nLayers);

    // Gravity link
    void applyGravityLink();

    // Expose SoA (with lock).
    const ParticleSoA& getSoA() const;
    const ParticleSoA getSoACopy() const;

    // Triangles, structure positions
    const std::vector<HexTriangle>& getHexTriangles() const;
    std::vector<glm::dvec3> getStructureParticlePositions() const;

    // Clear the sim
    void clearSimulation();

    // Async
    void startAsyncUpdates();
    void stopAsyncUpdates();

    // Update hex triangles
    void updateHexTriangles();

    // concurrency info
    std::atomic<int> effectiveStepsPerSecond { 0 };
    std::atomic<double> lastPhysicsUpdateTime { 0.0 };

    size_t getSpringCount() const;
    const std::vector<SpringData>& getSprings() const;

    void dropStructure();

private:
    void asyncLoop();
    void resolveExternalCollisions();

    ParticleSoA soA;
    std::vector<SpringData> springs;

    Link* gravityLink = nullptr;

    // Now double-based
    double springConstant = 0.5;
    bool bothEndsStatic   = false;
    double impulseScaling = 1000.0;

    mutable std::recursive_mutex simulationMutex;
    std::atomic<bool> asyncRunning { false };
    std::thread asyncThread;

    // Thread pool
    class ThreadPool;
    ThreadPool* threadPool = nullptr;

    // For hex
    std::vector<std::vector<glm::vec3>> hexagonVertexLists;
    std::vector<std::vector<int>> hexagonIndices;
    std::vector<HexTriangle> hexTriangles;

    glm::mat4 createRotationMatrix(double orientationDegrees);
    void generateHexagonCells(int numHexagons, double hexagonSize, const glm::mat4& rotationMatrix,
                              std::vector<glm::vec3>& uniquePositions,
                              std::set<std::pair<int,int>>& edgeSet);
    void assignUniquePositionsToSoA(const std::vector<glm::vec3>& uniquePositions, bool bothEndsStatic);
    void createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, double springRestLength);
};

#endif