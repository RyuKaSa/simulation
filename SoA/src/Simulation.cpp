#include "Simulation.hpp"
#include "SimulationSoAInternals.hpp"
#include <thread>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <queue>
#include <condition_variable>
#include <future>
#include <stdexcept>
#ifdef _OPENMP
#include <omp.h>
#endif

extern double getCurrentTime();

// --------------------
// ThreadPool Implementation
// --------------------
class Simulation::ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                        if (stop && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

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

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto &worker : workers) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// --------------------
// Core Simulation Functions
// --------------------

Simulation::Simulation() {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 2;
    }
    threadPool = new ThreadPool(numThreads);
    std::cout << "Number of threads: " << numThreads << std::endl;

    soA.position.reserve(300000);
    soA.velocity.reserve(300000);
    soA.forceAccum.reserve(300000);
    soA.mass.reserve(300000);
    soA.type.reserve(300000);
    soA.isStatic.reserve(300000);
    soA.color.reserve(300000);
    soA.dimensions.reserve(300000);

    Initialization();
}

Simulation::~Simulation() {
    clearSimulation();
    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
    if (threadPool) {
        delete threadPool;
        threadPool = nullptr;
    }
}

void Simulation::Initialization() {
    // (Uncomment the desired creation function call as needed.)
    // createMultiLayerHexGrid(5, 1.0, 1.0, true, 90.0, 1.0, 2);
    // createCord(10, 10.0, 1.0, true);
    // createHexGrid(5, 1.0, 1.0, true, 90.0);
    // createSquareGridWithDiagonals(10, 1.0, 1.0);
    createMultiLayerSquareGridWithDiagonals(200, 3, 1.0, 1.0, 1.0);

    if (gravityLink) {
        delete gravityLink;
    }
    gravityLink = new Link(soA, glm::dvec3(0.0, -9.81, 0.0));
    addStaticCubeUnderGrid();
}

void Simulation::reset() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "RESET initialized" << std::endl;
    update(0.0);
    clearSimulation();
    Initialization();
}

void Simulation::clearSimulation() {
    std::cout << "Clearing simulation..." << std::endl;
    soA.position.clear();
    soA.velocity.clear();
    soA.forceAccum.clear();
    soA.mass.clear();
    soA.type.clear();
    soA.isStatic.clear();
    soA.color.clear();
    soA.dimensions.clear();

    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

void Simulation::setSpringConstant(double k) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    springConstant = k;
    for (auto &sp : springs) {
        sp.springConstant = k;
    }
}

void Simulation::setDampingCoefficient(double z) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    for (auto &sp : springs) {
        sp.damping = z;
    }
}

void Simulation::dropStructure() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "Dropping structure: converting static particles to dynamic.\n";
    for (size_t i = 0; i < soA.isStatic.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE && soA.isStatic[i]) {
            soA.isStatic[i] = false;
        }
    }
}

void Simulation::update(double dt) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 2;
    }
    size_t totalSprings = springs.size();
    size_t chunkSize = (totalSprings + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;

    for (auto &f : soA.forceAccum) {
        f = glm::dvec3(0.0);
    }

    // 1) Spring force calculation
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, totalSprings);
        futures.push_back(
            threadPool->enqueue([this, start, end]() {
                #pragma clang loop vectorize(enable) interleave(enable)
                for (size_t i = start; i < end; i++) {
                    auto &sp = springs[i];
                    int i1 = sp.p1Index;
                    int i2 = sp.p2Index;
                    glm::dvec3 pos1 = soA.position[i1];
                    glm::dvec3 pos2 = soA.position[i2];
                    glm::dvec3 vel1 = soA.velocity[i1];
                    glm::dvec3 vel2 = soA.velocity[i2];

                    double dist = glm::distance(pos1, pos2);
                    if (dist < 1e-7) continue;

                    glm::dvec3 dir = (pos1 - pos2) / dist;
                    double k = sp.springConstant;
                    double rest = sp.restLength;
                    glm::dvec3 springForce = -k * (dist - rest) * dir;

                    double z = sp.damping;
                    glm::dvec3 dampingForce = -z * (vel1 - vel2);

                    glm::dvec3 totalForce = springForce + dampingForce;

                    soA.forceAccum[i1] += totalForce;
                    soA.forceAccum[i2] -= totalForce;
                }
            })
        );
    }
    for (auto &f : futures) {
        f.get();
    }
    futures.clear();

    // 2) Gravity
    if (gravityLink) {
        applyGravityLink();
    }

    // 3) Integration
    size_t n = soA.position.size();
    size_t chunkPart = (n + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkPart;
        size_t end = std::min(start + chunkPart, n);
        futures.push_back(
            threadPool->enqueue([this, dt, start, end]() {
                #pragma omp simd
                for (size_t i = start; i < end; i++) {
                    if (soA.isStatic[i]) {
                        soA.forceAccum[i] = glm::dvec3(0.0);
                    } else {
                        double m = soA.mass[i];
                        glm::dvec3 accel = soA.forceAccum[i] / m;
                        soA.velocity[i] += accel * dt;
                        soA.position[i] += soA.velocity[i] * dt;
                        soA.forceAccum[i] = glm::dvec3(0.0);
                    }
                }
            })
        );
    }
    for (auto &f : futures) {
        f.get();
    }
    futures.clear();

    // 4) External collisions
    resolveExternalCollisions();
}

void Simulation::resolveExternalCollisions() {
    const double restitution = 0.5;
    const double particleRadius = 0.1;
    for (size_t cubeIndex = 0; cubeIndex < soA.position.size(); cubeIndex++) {
        if (soA.type[cubeIndex] != ParticleType::EXTERNAL) {
            continue;
        }
        glm::dvec3 cubeCenter = soA.position[cubeIndex];
        glm::dvec3 halfExtents = soA.dimensions[cubeIndex] * 0.5;

        for (size_t i = 0; i < soA.position.size(); i++) {
            if (soA.type[i] != ParticleType::STRUCTURE) continue;
            if (soA.isStatic[i]) continue;
            glm::dvec3 pos = soA.position[i];
            glm::dvec3 closest;
            for (int j = 0; j < 3; j++) {
                double cmin = cubeCenter[j] - halfExtents[j];
                double cmax = cubeCenter[j] + halfExtents[j];
                if (pos[j] < cmin)      closest[j] = cmin;
                else if (pos[j] > cmax) closest[j] = cmax;
                else                    closest[j] = pos[j];
            }
            glm::dvec3 diff = pos - closest;
            double dist = glm::length(diff);
            if (dist < particleRadius) {
                double penetration = particleRadius - dist;
                glm::dvec3 normal;
                if (dist > 1e-6) {
                    normal = diff / dist;
                } else {
                    normal = glm::dvec3(0, 1, 0);
                }
                soA.position[i] += normal * penetration;
                double vn = glm::dot(soA.velocity[i], normal);
                if (vn < 0) {
                    soA.velocity[i] -= (1.0 + restitution) * vn * normal;
                }
            }
        }
    }
}

void Simulation::applyGravityLink() {
    if (gravityLink) {
        gravityLink->applyGravity();
    }
}

void Simulation::startAsyncUpdates() {
    asyncRunning = true;
    asyncThread = std::thread(&Simulation::asyncLoop, this);
}

void Simulation::stopAsyncUpdates() {
    asyncRunning = false;
    if (asyncThread.joinable()) {
        asyncThread.join();
    }
}

void Simulation::asyncLoop() {
    double lastTime = getCurrentTime();
    double lastMeasure = lastTime;
    int stepsCount = 0;
    while (asyncRunning) {
        double now = getCurrentTime();
        double dt = now - lastTime;
        lastTime = now;
        {
            std::lock_guard<std::recursive_mutex> lck(simulationMutex);
            update(dt);
        }
        stepsCount++;
        lastPhysicsUpdateTime.store(dt);
        if ((now - lastMeasure) >= 1.0) {
            effectiveStepsPerSecond.store(stepsCount);
            stepsCount = 0;
            lastMeasure = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

const ParticleSoA& Simulation::getSoA() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return soA;
}

const ParticleSoA Simulation::getSoACopy() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return soA;
}

const std::vector<HexTriangle>& Simulation::getHexTriangles() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return hexTriangles;
}

std::vector<glm::dvec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::dvec3> ret;
    ret.reserve(soA.position.size());
    for (size_t i = 0; i < soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            ret.push_back(soA.position[i]);
        }
    }
    return ret;
}

size_t Simulation::getSpringCount() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return springs.size();
}

const std::vector<SpringData>& Simulation::getSprings() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return springs;
}