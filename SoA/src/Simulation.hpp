#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <vector>
#include <glm/glm.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <set>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#include <future>
#include <queue>
#include <condition_variable>
#include <stdexcept>
#include <iostream>

#include "PMat.hpp"
#include "Link.hpp"
#include "SimulationSoAInternals.hpp"
#include "GUI.hpp"
#include "Camera.hpp"

// For manual SIMD logic
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif
#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

//-----------------------------------------------------------
// HexTriangle: simple triangle with vertices and normal
//-----------------------------------------------------------
struct HexTriangle {
    glm::vec3 vertices[3];
    glm::vec3 normal;
};

//-----------------------------------------------------------
// SharedSimParams: Holds parameters used across simulations
//-----------------------------------------------------------
struct SharedSimParams {
    double springConstant     = 3000.0;
    double dampingCoefficient = 70.0;
    int    gridSize           = 100;    // for cloth or environment grid
    double springRestLength   = 1.0;
    double gravityStrength    = 9.81;
};

//-----------------------------------------------------------
// SimulationBase: Abstract base class for simulation
//-----------------------------------------------------------
class SimulationBase {
public:
    SimulationBase();
    virtual ~SimulationBase();

    // Called once after construction to build the scene.
    virtual void Initialization() = 0; // Pure virtual

    // Reset simulation – default implementation clears and reinitializes.
    virtual void reset();

    // Update physics by dt seconds.
    virtual void update(double dt);

    // Start/stop background async updates.
    void startAsyncUpdates();
    void stopAsyncUpdates();

    void applyThreadedSpringForces(double dt);
    void applyGravityLink();

    // Accessors.
    const ParticleSoA& getSoA() const;
    ParticleSoA getSoACopy() const;
    size_t getSpringCount() const;
    const std::vector<SpringData>& getSprings() const;
    std::vector<SpringData>& getSpringsReference() { return springs; }

    // Returns a mutable reference to the ParticleSoA.
    ParticleSoA& getSoAReference() { return soA; }

    // Clears external (block) particles from the simulation.
    void clearExternalBlocks();

    // setters.
    void setSpringConstant(double k);
    void setDampingCoefficient(double z);

    // Functions used by Renderer.
    virtual const std::vector<HexTriangle>& getHexTriangles() const;
    virtual std::vector<glm::dvec3> getStructureParticlePositions() const;

    // Creation functions – these were part of your original Simulation.
    virtual void addStaticCubeUnderGrid();
    virtual void createCord(int numBalls, double length, double springRestLength, bool bothEndsStatic);
    virtual void createSquareGridWithDiagonals(int gridSize, double spacing, double springRestLength);
    virtual void createMultiLayerSquareGridWithDiagonals(int gridSize, int nLayers,
                                                        double cellSize, double layerSpacing,
                                                        double springRestLength);
    virtual void createHexGrid(int numHexagons, double hexagonSize, double springRestLength,
                               bool bothEndsStatic, double orientationDegrees);
    virtual void createMultiLayerHexGrid(int numHexagons, double hexagonSize, double springRestLength,
                                         bool bothEndsStatic, double orientationDegrees, double layerHeight, int nLayers);
    virtual void createMultiLayerSquareGridWithDiagonalsCentered(int cx, int cy, int cz, int gridSize, int nLayers, double cellSize, double layerSpacing, double springRestLength);

    // Helper creation functions.
    virtual glm::dmat4 createRotationMatrix(double orientationDegrees);
    virtual void generateHexagonCells(int numHexagons, double hexagonSize,
                                      const glm::dmat4& rotationMatrix,
                                      std::vector<glm::dvec3>& uniquePositions,
                                      std::set<std::pair<int,int>>& edgeSet);
    virtual void assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions, bool bothEndsStatic);
    virtual void createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, double springRestLength);
    virtual bool approxEqualVec3(const glm::dvec3& a, const glm::dvec3& b, double epsilon = 0.00001);
    virtual int findApproxVertexIndex(const std::vector<glm::dvec3>& vertices, const glm::dvec3& target, double epsilon = 0.00001);

    // Other functions.
    void dropStructure();
    void clearSimulation();

    // GUI pointer.
    void setGUIInstance(GUI* gui) { guiInstance = gui; }

    // Performance metrics.
    double getLastPhysicsUpdateTime() const { return lastPhysicsUpdateTime.load(); }
    int getEffectiveStepsPerSecond() const { return effectiveStepsPerSecond.load(); }

    // Shared parameters.
    SharedSimParams sharedParams;

protected:
    // Simulation data.
    ParticleSoA soA;
    std::vector<SpringData> springs;
    std::vector<HexTriangle> hexTriangles;

    // Additional members from original Simulation.
    bool bothEndsStatic = false;
    std::vector<std::vector<glm::dvec3>> hexagonVertexLists;
    std::vector<std::vector<int>> hexagonIndices;

    // Gravity link.
    Link* gravityLink = nullptr;

    // Concurrency.
    std::thread asyncThread;
    std::atomic<bool> asyncRunning { false };
    mutable std::recursive_mutex simulationMutex;
    std::atomic<int> effectiveStepsPerSecond { 0 };
    std::atomic<double> lastPhysicsUpdateTime { 0.0 };

    GUI* guiInstance = nullptr;

    //------------------------------------
    // ThreadPool nested class.
    //------------------------------------
    class ThreadPool {
    public:
        ThreadPool(size_t numThreads);
        ~ThreadPool();

        template<typename F>
        auto enqueue(F&& f) -> std::future<decltype(f())> {
            using return_type = decltype(f());
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (stop)
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                tasks.emplace([task](){ (*task)(); });
            }
            condition.notify_one();
            return res;
        }
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop = false;
    };

    ThreadPool* threadPool = nullptr;

    // Derived classes can override if needed:
    virtual void resolveExternalCollisions();
    virtual void asyncLoop();
};

//
// Derived classes for the two scenes:
//

class ClothSimulation : public SimulationBase {
public:
    ClothSimulation(GUI* gui = nullptr) {
        if (gui) setGUIInstance(gui);
    }
    virtual ~ClothSimulation() {}
    virtual void Initialization() override;
    virtual void reset() override {
        this->SimulationBase::reset();
    }
};

class EnvironmentSimulation : public SimulationBase {
public:
    EnvironmentSimulation(GUI* gui = nullptr) {
        if (gui) setGUIInstance(gui);
    }
    virtual ~EnvironmentSimulation() {}
    virtual void Initialization() override;
    virtual void reset() override {
        this->SimulationBase::reset();
    }
    virtual void removeAllCloths();
};

#endif // SIMULATION_HPP