#include "Simulation.hpp"
#include "SimulationSoAInternals.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#ifdef _OPENMP
#include <omp.h>
#endif

extern double getCurrentTime();

// ------------------- SimulationBase Implementation -------------------

SimulationBase::SimulationBase() {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if(numThreads == 0) {
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
}

SimulationBase::~SimulationBase() {
    stopAsyncUpdates();
    clearSimulation();
    if(gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
    if(threadPool) {
        delete threadPool;
        threadPool = nullptr;
    }
}

void SimulationBase::reset() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "SimulationBase::reset()" << std::endl;
    clearSimulation();
    Initialization();
}

void SimulationBase::update(double dt) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    // 1) Zero out force accumulators
    for(auto &f : soA.forceAccum) {
        f = glm::dvec3(0.0);
    }

    // 2) Springs
    // (Threaded spring force calculation)
    // Here, dt is not directly used, but you may modify if needed.
    // We call our helper:
    // (See definition below)
    // In our example, we pass dt to our threaded spring force function:
    applyThreadedSpringForces(dt);

    // 3) Gravity
    if(gravityLink) {
        applyGravityLink();
    }

    // 4) Integration
    size_t n = soA.position.size();
    unsigned int numThreads = std::thread::hardware_concurrency();
    if(numThreads == 0) numThreads = 2;
    size_t chunkPart = (n + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;
    for(unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkPart;
        size_t end = std::min(start + chunkPart, n);
        futures.push_back(
            threadPool->enqueue([this, dt, start, end]() {
                #ifdef _OPENMP
                #pragma omp simd
                #endif
                for(size_t i = start; i < end; i++) {
                    if(soA.isStatic[i]) {
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
    for(auto &fut : futures) {
        fut.get();
    }
    futures.clear();

    // 5) External collisions
    resolveExternalCollisions();
}

void SimulationBase::startAsyncUpdates() {
    asyncRunning = true;
    asyncThread = std::thread(&SimulationBase::asyncLoop, this);
}

void SimulationBase::stopAsyncUpdates() {
    asyncRunning = false;
    if(asyncThread.joinable()) {
        asyncThread.join();
    }
}

void SimulationBase::asyncLoop() {
    double lastTime = getCurrentTime();
    double lastMeasure = lastTime;
    int stepsCount = 0;
    while(asyncRunning) {
        double now = getCurrentTime();
        double dt = now - lastTime;
        lastTime = now;
        {
            std::lock_guard<std::recursive_mutex> lck(simulationMutex);
            update(dt);
        }
        stepsCount++;
        lastPhysicsUpdateTime.store(dt);
        if((now - lastMeasure) >= 1.0) {
            effectiveStepsPerSecond.store(stepsCount);
            stepsCount = 0;
            lastMeasure = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

const ParticleSoA& SimulationBase::getSoA() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return soA;
}

ParticleSoA SimulationBase::getSoACopy() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return soA;
}

size_t SimulationBase::getSpringCount() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return springs.size();
}

const std::vector<SpringData>& SimulationBase::getSprings() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return springs;
}

const std::vector<HexTriangle>& SimulationBase::getHexTriangles() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return hexTriangles;
}

std::vector<glm::dvec3> SimulationBase::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::dvec3> ret;
    ret.reserve(soA.position.size());
    for(size_t i = 0; i < soA.position.size(); i++) {
        if(soA.type[i] == ParticleType::STRUCTURE)
            ret.push_back(soA.position[i]);
    }
    return ret;
}

void SimulationBase::setSpringConstant(double k) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    sharedParams.springConstant = k;
    for(auto &sp : springs) {
        sp.springConstant = k;
    }
}

void SimulationBase::setDampingCoefficient(double z) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    sharedParams.dampingCoefficient = z;
    for(auto &sp : springs) {
        sp.damping = z;
    }
}

void SimulationBase::applyGravityLink() {
    if(gravityLink)
        gravityLink->applyGravity();
}

void SimulationBase::dropStructure() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "Dropping structure: converting static particles to dynamic.\n";
    for(size_t i = 0; i < soA.isStatic.size(); i++) {
        if(soA.type[i] == ParticleType::STRUCTURE && soA.isStatic[i])
            soA.isStatic[i] = false;
    }
}

void SimulationBase::clearSimulation() {
    std::cout << "SimulationBase::clearSimulation()...\n";
    soA.position.clear();
    soA.velocity.clear();
    soA.forceAccum.clear();
    soA.mass.clear();
    soA.type.clear();
    soA.isStatic.clear();
    soA.color.clear();
    soA.dimensions.clear();
    springs.clear();
    hexTriangles.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    if(gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

// ------------------- ThreadPool Implementation -------------------
SimulationBase::ThreadPool::ThreadPool(size_t numThreads) {
    stop = false;
    for (size_t i = 0; i < numThreads; i++) {
        workers.emplace_back([this]() {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
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

SimulationBase::ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (auto &worker : workers) {
        worker.join();
    }
}

// Apply spring forces in a multi-threaded manner:
void SimulationBase::applyThreadedSpringForces(double /*dt*/) {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;
    size_t totalSprings = springs.size();
    size_t chunkSize = (totalSprings + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, totalSprings);
        futures.push_back(
            threadPool->enqueue([this, start, end]() {
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
}

// Basic external collision logic.
void SimulationBase::resolveExternalCollisions() {
    const double restitution = 0.5;
    const double frictionCoefficient = 0.3;
    const double particleRadius = 0.1;
    for (size_t cubeIndex = 0; cubeIndex < soA.position.size(); cubeIndex++) {
        if (soA.type[cubeIndex] != ParticleType::EXTERNAL)
            continue;
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
                glm::dvec3 normal = (dist > 1e-6) ? diff / dist : glm::dvec3(0, 1, 0);
                soA.position[i] += normal * penetration;
                double vn = glm::dot(soA.velocity[i], normal);
                if (vn < 0) {
                    soA.velocity[i] -= (1.0 + restitution) * vn * normal;
                }
                glm::dvec3 normalComponent = glm::dot(soA.velocity[i], normal) * normal;
                glm::dvec3 tangentialComponent = soA.velocity[i] - normalComponent;
                soA.velocity[i] = normalComponent + (1.0 - frictionCoefficient)*tangentialComponent;
            }
        }
    }
}

// ------------------- Derived Class Implementations -------------------

// ClothSimulation Implementation.
void ClothSimulation::Initialization() {
    if (!guiInstance) {
        std::cerr << "ClothSimulation: no GUI instance, skipping.\n";
        return;
    }
    sharedParams.gridSize = guiInstance->getGridSize();
    sharedParams.springRestLength = guiInstance->getSpringRestLength();
    sharedParams.gravityStrength = guiInstance->getGravityStrength();
    sharedParams.springConstant = guiInstance->getSpringConstant();
    sharedParams.dampingCoefficient = guiInstance->getDampingCoefficient();

    createMultiLayerSquareGridWithDiagonals(sharedParams.gridSize, 2, 0.03, 0.03, sharedParams.springRestLength);

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
    gravityLink = new Link(soA, glm::dvec3(0.0, -sharedParams.gravityStrength, 0.0));

    addStaticCubeUnderGrid();
}

// EnvironmentSimulation Implementation.
void EnvironmentSimulation::Initialization() {
    if (!guiInstance) {
        std::cerr << "EnvironmentSimulation: no GUI instance, skipping.\n";
        return;
    }
    sharedParams.gridSize = guiInstance->getGridSize();
    sharedParams.springRestLength = guiInstance->getSpringRestLength();
    sharedParams.gravityStrength = guiInstance->getGravityStrength();
    sharedParams.springConstant = guiInstance->getSpringConstant();
    sharedParams.dampingCoefficient = guiInstance->getDampingCoefficient();

    createHexGrid(sharedParams.gridSize, 0.03, sharedParams.springRestLength, true, 0.0);

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
    gravityLink = new Link(soA, glm::dvec3(3.0, -sharedParams.gravityStrength, 0.0));
}