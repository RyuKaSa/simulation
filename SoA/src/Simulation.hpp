#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <vector>
#include <glm/glm.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <set>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>

// Include your necessary headers (assuming they exist).
#include "PMat.hpp"
#include "Link.hpp"
#include "SimulationSoAInternals.hpp"

// For manual SIMD logic
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h> // AVX intrinsics
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>  // NEON intrinsics
#endif

struct HexTriangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
};

class Simulation {
public:
    // ------------------- Core Simulation Functions -------------------
    Simulation();
    ~Simulation();

    void Initialization();
    void reset();
    void update(double dt);

    void setSpringConstant(double k);
    void setDampingCoefficient(double z);
    double getImpulseScaling() const { return impulseScaling; }
    void setImpulseScaling(double scaling) { impulseScaling = scaling; }

    void applyGravityLink();

    const ParticleSoA& getSoA() const;
    const ParticleSoA getSoACopy() const;

    const std::vector<HexTriangle>& getHexTriangles() const;
    std::vector<glm::dvec3> getStructureParticlePositions() const;

    void clearSimulation();

    void startAsyncUpdates();
    void stopAsyncUpdates();

    void updateHexTriangles();

    size_t getSpringCount() const;
    const std::vector<SpringData>& getSprings() const;

    void dropStructure();

    // ------------------- Creation Functions -------------------
    void addStaticCubeUnderGrid();
    void createCord(int numBalls, double length, double springRestLength, bool bothEndsStatic);
    void createHexGrid(int numHexagons, double hexagonSize, double springRestLength,
                       bool bothEndsStatic, double orientationDegrees);
    void createSquareGridWithDiagonals(int gridSize, double spacing, double springRestLength);
    void createMultiLayerSquareGridWithDiagonals(int gridSize, int nLayers, double cellSize,
                                                 double layerSpacing, double springRestLength);
    void createMultiLayerHexGrid(int numHexagons, double hexagonSize, double springRestLength,
                                 bool bothEndsStatic, double orientationDegrees, double layerHeight, int nLayers);

    double getLastPhysicsUpdateTime() const { return lastPhysicsUpdateTime.load(); }
    int getEffectiveStepsPerSecond() const { return effectiveStepsPerSecond.load(); }

private:
    // ------------------- Internal Helper Functions -------------------
    void asyncLoop();
    void resolveExternalCollisions();

    // Creation helper functions
    glm::dmat4 createRotationMatrix(double orientationDegrees);
    void generateHexagonCells(int numHexagons, double hexagonSize,
                              const glm::dmat4& rotationMatrix,
                              std::vector<glm::dvec3>& uniquePositions,
                              std::set<std::pair<int,int>>& edgeSet);
    void assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions, bool bothEndsStatic);
    void createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, double springRestLength);
    bool approxEqualVec3(const glm::dvec3& a, const glm::dvec3& b, double epsilon = 0.00001);
    int findApproxVertexIndex(const std::vector<glm::dvec3>& vertices, const glm::dvec3& target, double epsilon = 0.00001);

    // ------------------- Member Variables -------------------
    ParticleSoA soA;                    // Positions, velocities, etc.
    std::vector<SpringData> springs;    // Spring information
    Link* gravityLink = nullptr;

    double springConstant = 0.5;
    bool bothEndsStatic   = false;
    double impulseScaling = 1000.0;

    mutable std::recursive_mutex simulationMutex;
    std::atomic<bool> asyncRunning { false };
    std::thread asyncThread;

    std::atomic<int> effectiveStepsPerSecond { 0 };
    std::atomic<double> lastPhysicsUpdateTime { 0.0 };

    class ThreadPool;
    ThreadPool* threadPool = nullptr;

    std::vector<std::vector<glm::dvec3>> hexagonVertexLists;
    std::vector<std::vector<int>> hexagonIndices;
    std::vector<HexTriangle> hexTriangles;
};

#endif // SIMULATION_HPP