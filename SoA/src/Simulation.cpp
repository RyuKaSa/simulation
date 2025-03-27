// Simulation.cpp
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

    // Reserve memory (note: reserve() does not change size; cloth and external particles
    // will be appended later via push_back or resize)
    soA.position.reserve(300000);
    soA.velocity.reserve(300000);
    soA.forceAccum.reserve(300000);
    soA.mass.reserve(300000);
    soA.type.reserve(300000);
    soA.isStatic.reserve(300000);
    soA.color.reserve(300000);
    soA.dimensions.reserve(300000);
    // Also reserve for clothID vector
    soA.clothID.reserve(300000);
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

    // 1) Zero out force accumulators for all particles
    for(auto &f : soA.forceAccum) {
        f = glm::dvec3(0.0);
    }

    // 2) Compute spring forces.
    // Springs will only be applied if both endpoints belong to the same cloth (clothID not -1)
    applyThreadedSpringForces(dt);

    // 3) Apply gravity if available.
    if(gravityLink) {
        applyGravityLink();
    }

    // 4) Integrate particle positions and velocities
    size_t n = soA.position.size();
    // size_t n = clothParticleCount;
    unsigned int numThreads = std::thread::hardware_concurrency();
    if(numThreads == 0) numThreads = 2;
    size_t chunkPart = (n + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;
    for(unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkPart;
        size_t end = std::min(start + chunkPart, n);
        futures.push_back(
            threadPool->enqueue([this, dt, start, end]() {
                for(size_t i = start; i < end; i++) {
                    if (soA.type[i] == ParticleType::BACKGROUND)
                        continue;
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

    // 5) Process external collisions (this function may handle collisions between external blocks
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
    soA.clothID.clear();
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

// Apply spring forces in a multi-threaded manner.
// Only apply springs for which both endpoints share the same clothID (and that clothID is not -1).
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
                    // Only process spring if both endpoints belong to the same cloth and are not external
                    if (soA.clothID[i1] != soA.clothID[i2] || soA.clothID[i1] < 0)
                        continue;
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

std::vector<float> SimulationBase::buildClothMeshData(int startIndex, int gridSize, int nLayers) const {
    std::vector<float> meshData;

    // Helper lambda: get the current particle position as glm::vec3.
    auto getPos = [this, startIndex, gridSize](int layer, int i, int j) -> glm::vec3 {
        size_t index = startIndex + (layer * gridSize * gridSize) + (i * gridSize) + j;
        return glm::vec3(soA.position[index]);  // cast from dvec3 to vec3
    };

    // Helper lambda: add a triangle to meshData.
    // Each triangle is three vertices; for each vertex, we store 6 floats: position (x,y,z) and normal (x,y,z).
    auto addTriangle = [&meshData](const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2) {
        glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        // Vertex 1
        meshData.push_back(p0.x); meshData.push_back(p0.y); meshData.push_back(p0.z);
        meshData.push_back(normal.x); meshData.push_back(normal.y); meshData.push_back(normal.z);
        // Vertex 2
        meshData.push_back(p1.x); meshData.push_back(p1.y); meshData.push_back(p1.z);
        meshData.push_back(normal.x); meshData.push_back(normal.y); meshData.push_back(normal.z);
        // Vertex 3
        meshData.push_back(p2.x); meshData.push_back(p2.y); meshData.push_back(p2.z);
        meshData.push_back(normal.x); meshData.push_back(normal.y); meshData.push_back(normal.z);
    };

    // ----------------------------
    // Top Face (upper layer)
    // ----------------------------
    int topLayer = nLayers - 1;
    for (int i = 0; i < gridSize - 1; i++) {
        for (int j = 0; j < gridSize - 1; j++) {
            glm::vec3 p00 = getPos(topLayer, i, j);
            glm::vec3 p01 = getPos(topLayer, i, j + 1);
            glm::vec3 p10 = getPos(topLayer, i + 1, j);
            glm::vec3 p11 = getPos(topLayer, i + 1, j + 1);
            // Two triangles per cell; winding so that normal points upward.
            addTriangle(p00, p01, p10);
            addTriangle(p01, p11, p10);
        }
    }

    // ----------------------------
    // Bottom Face (lower layer)
    // ----------------------------
    int bottomLayer = 0;
    for (int i = 0; i < gridSize - 1; i++) {
        for (int j = 0; j < gridSize - 1; j++) {
            glm::vec3 p00 = getPos(bottomLayer, i, j);
            glm::vec3 p01 = getPos(bottomLayer, i, j + 1);
            glm::vec3 p10 = getPos(bottomLayer, i + 1, j);
            glm::vec3 p11 = getPos(bottomLayer, i + 1, j + 1);
            // Reverse winding order so that the normal points downward.
            addTriangle(p00, p10, p01);
            addTriangle(p01, p10, p11);
        }
    }

    // ----------------------------
    // Side Faces (vertical sides)
    // ----------------------------
    // For each pair of consecutive layers, generate quads along the perimeter and split each quad into two triangles.
    for (int l = 0; l < nLayers - 1; l++) {
        // Lambda to add a quad (split into 2 triangles)
        auto addQuad = [&](const glm::vec3 &A, const glm::vec3 &B,
                           const glm::vec3 &C, const glm::vec3 &D) {
            addTriangle(A, B, C);
            addTriangle(B, D, C);
        };

        // Front edge (i = 0): assuming front faces in -Z direction.
        {
            int i = 0;
            for (int j = 0; j < gridSize - 1; j++) {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i, j + 1);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i, j + 1);
                addQuad(A, B, C, D);
            }
        }
        // Back edge (i = gridSize - 1): reverse winding for outward normal.
        {
            int i = gridSize - 1;
            for (int j = 0; j < gridSize - 1; j++) {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i, j + 1);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i, j + 1);
                addQuad(A, C, B, D);
            }
        }
        // Left edge (j = 0)
        {
            int j = 0;
            for (int i = 0; i < gridSize - 1; i++) {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i + 1, j);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i + 1, j);
                addQuad(A, B, C, D);
            }
        }
        // Right edge (j = gridSize - 1): reverse winding.
        {
            int j = gridSize - 1;
            for (int i = 0; i < gridSize - 1; i++) {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i + 1, j);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i + 1, j);
                addQuad(A, C, B, D);
            }
        }
    }

    return meshData;
}

// Basic external collision logic.
void SimulationBase::resolveExternalCollisions() {
    const double restitution = 0.5;
    const double frictionCoefficient = 0.3;
    const double particleRadius = 0.05;
    for (size_t cubeIndex = 0; cubeIndex < soA.position.size(); cubeIndex++) {
        if (soA.type[cubeIndex] == ParticleType::BACKGROUND)
            continue;
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
                soA.velocity[i] = normalComponent + (1.0 - frictionCoefficient) * tangentialComponent;
            }
        }
    }
}

void SimulationBase::clearExternalBlocks() {
    // Create new vectors using the same allocator for glm::dvec3 vectors.
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> newPositions;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> newVelocities;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> newForceAccum;
    std::vector<double> newMass;
    std::vector<ParticleType> newType;
    std::vector<bool> newIsStatic;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> newColor;
    std::vector<glm::dvec3, AlignedAllocator<glm::dvec3, 32>> newDimensions;
    std::vector<int> newClothID;

    // Keep cloth particles (indices where clothID >= 0) and reappend external ones.
    size_t total = soA.position.size();
    for (size_t i = 0; i < total; i++) {
        if (soA.clothID[i] != -1) { // cloth particle
            newPositions.push_back(soA.position[i]);
            newVelocities.push_back(soA.velocity[i]);
            newForceAccum.push_back(soA.forceAccum[i]);
            newMass.push_back(soA.mass[i]);
            newType.push_back(soA.type[i]);
            newIsStatic.push_back(soA.isStatic[i]);
            newColor.push_back(soA.color[i]);
            newDimensions.push_back(soA.dimensions[i]);
            newClothID.push_back(soA.clothID[i]);
        }
    }
    // Append external blocks (clothID == -1)
    for (size_t i = 0; i < total; i++) {
        if (soA.clothID[i] == -1) {
            newPositions.push_back(soA.position[i]);
            newVelocities.push_back(soA.velocity[i]);
            newForceAccum.push_back(soA.forceAccum[i]);
            newMass.push_back(soA.mass[i]);
            newType.push_back(soA.type[i]);
            newIsStatic.push_back(soA.isStatic[i]);
            newColor.push_back(soA.color[i]);
            newDimensions.push_back(soA.dimensions[i]);
            newClothID.push_back(soA.clothID[i]);
        }
    }

    soA.position    = newPositions;
    soA.velocity    = newVelocities;
    soA.forceAccum  = newForceAccum;
    soA.mass        = newMass;
    soA.type        = newType;
    soA.isStatic    = newIsStatic;
    soA.color       = newColor;
    soA.dimensions  = newDimensions;
    soA.clothID     = newClothID;
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

    

    // Choose structure based on the selected dropdown item.
    if (guiInstance->getSelectedStructure() == 0) {
        createMultiLayerSquareGridWithDiagonals(sharedParams.gridSize, guiInstance->getNumberLayers(), 0.03, 0.03, sharedParams.springRestLength);
    } else if (guiInstance->getSelectedStructure() == 1) {
        createSquareGridWithDiagonals(sharedParams.gridSize, 0.03, sharedParams.springRestLength);
    } else if (guiInstance->getSelectedStructure() == 2) {
        createMultiLayerHexGrid(sharedParams.gridSize, 0.03, sharedParams.springRestLength, true, 90, 0.03, guiInstance->getNumberLayers());
    } else if (guiInstance->getSelectedStructure() == 3) {
        createHexGrid(sharedParams.gridSize, 0.03, sharedParams.springRestLength, true, 90.0);
    } else if (guiInstance->getSelectedStructure() == 4) {
        createCord(sharedParams.gridSize, 2, sharedParams.springRestLength, true);
    } else {
        std::cerr << "ClothSimulation: unknown structure type, skipping.\n";
    }

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

    // Create a hex grid for the environment cloth.
    // createHexGrid(sharedParams.gridSize, 0.03, sharedParams.springRestLength, true, 0.0);

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
    gravityLink = new Link(soA, glm::dvec3(0.0, -sharedParams.gravityStrength, 0.0));
}

void EnvironmentSimulation::removeAllCloths()
{
    ParticleSoA &soa = getSoAReference();
    std::vector<SpringData> &springs = getSpringsReference();
    std::cout << "Removing all cloth particles...\n";
    // Iterate backwards so removal indices don't shift
    for (int i = (int)soa.position.size() - 1; i >= 0; i--)
    {
        // If clothID >= 0, that means it's cloth
        if (soa.clothID[i] >= 0)
        {
            // removeParticle is the inline utility in SimulationSoAInternals.hpp
            removeParticle(soa, springs, i);
        }
    }
}