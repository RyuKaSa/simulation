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

// ------------------- Creation Functions -------------------

void SimulationBase::addStaticCubeUnderGrid() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    glm::dvec3 center(0.0);
    int count = 0;
    for(size_t i = 0; i < soA.position.size(); i++) {
        if(soA.type[i] == ParticleType::STRUCTURE) {
            center += soA.position[i];
            count++;
        }
    }
    if(count > 0)
        center /= static_cast<double>(count);
    else
        center = glm::dvec3(0.0);
    double cubeSize = 1.0;
    glm::dvec3 cubePos = center + glm::dvec3(0.0, -cubeSize - 0.5, 0.0);
    
    soA.position.push_back(cubePos);
    soA.velocity.push_back(glm::dvec3(0.0));
    soA.forceAccum.push_back(glm::dvec3(0.0));
    soA.mass.push_back(100.0);
    soA.type.push_back(ParticleType::EXTERNAL);
    soA.isStatic.push_back(true);
    soA.color.push_back(glm::dvec3(0.56, 1.0, 0.4));
    soA.dimensions.push_back(glm::dvec3(cubeSize));
}

void SimulationBase::createCord(int numBalls, double length, double springRestLength, bool bothEndsStatic_) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;
    soA.position.reserve(numBalls);
    soA.velocity.reserve(numBalls);
    soA.forceAccum.reserve(numBalls);
    soA.mass.reserve(numBalls);
    soA.type.reserve(numBalls);
    soA.isStatic.reserve(numBalls);
    soA.color.reserve(numBalls);
    soA.dimensions.reserve(numBalls);
    
    double spacing = length / (numBalls - 1);
    glm::dvec3 startPos(-length/2, 0.0, 0.0);
    
    for (int i = 0; i < numBalls; i++) {
        glm::dvec3 pos = startPos + glm::dvec3(i * spacing, 0.0, 0.0);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::dvec3(0.0));
        soA.forceAccum.push_back(glm::dvec3(0.0));
        soA.mass.push_back(10.0);
        soA.type.push_back(ParticleType::STRUCTURE);
        bool sflag = (bothEndsStatic_) ? (i == 0 || i == (numBalls - 1)) : (i == 0);
        soA.isStatic.push_back(sflag);
        soA.color.push_back(glm::dvec3(1,0,0));
        soA.dimensions.push_back(glm::dvec3(3.0));
    }
    for (int i = 0; i < numBalls - 1; i++) {
        double dist = glm::distance(soA.position[i], soA.position[i+1]);
        SpringData sp;
        sp.p1Index = i;
        sp.p2Index = i+1;
        sp.restLength = dist * springRestLength;
        sp.springConstant = sharedParams.springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

void SimulationBase::createSquareGridWithDiagonals(int gridSize, double cellSize, double springRestLength) {
    clearSimulation();
    
    int numParticles = gridSize * gridSize;
    soA.position.resize(numParticles);
    soA.velocity.resize(numParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(numParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(numParticles, massValue);
    soA.type.resize(numParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(numParticles, false);
    soA.color.resize(numParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(numParticles, glm::dvec3(1.0));
    
    double halfWidth = (gridSize - 1) * cellSize / 2.0;
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            int idx = i * gridSize + j;
            soA.position[idx] = glm::dvec3(j * cellSize - halfWidth, 0.0, i * cellSize - halfWidth);
        }
    }
    
    for (int i = 0; i < gridSize; i++) {
        int leftIdx = i * gridSize;
        int rightIdx = i * gridSize + (gridSize - 1);
        soA.isStatic[leftIdx] = true;
        soA.isStatic[rightIdx] = true;
    }
    
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            int idx = i * gridSize + j;
            if (j < gridSize - 1) {
                int rightIdx = i * gridSize + (j + 1);
                double dist = glm::distance(soA.position[idx], soA.position[rightIdx]);
                SpringData sp;
                sp.p1Index = idx;
                sp.p2Index = rightIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            if (i < gridSize - 1) {
                int bottomIdx = (i + 1) * gridSize + j;
                double dist = glm::distance(soA.position[idx], soA.position[bottomIdx]);
                SpringData sp;
                sp.p1Index = idx;
                sp.p2Index = bottomIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            if (i < gridSize - 1 && j < gridSize - 1) {
                int diagIdx = (i + 1) * gridSize + (j + 1);
                double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                SpringData sp;
                sp.p1Index = idx;
                sp.p2Index = diagIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            if (i < gridSize - 1 && j > 0) {
                int diagIdx = (i + 1) * gridSize + (j - 1);
                double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                SpringData sp;
                sp.p1Index = idx;
                sp.p2Index = diagIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
        }
    }
}

void SimulationBase::createMultiLayerSquareGridWithDiagonals(int gridSize, int nLayers, double cellSize, double layerSpacing, double springRestLength) {
    clearSimulation();
    
    int particlesPerLayer = gridSize * gridSize;
    int totalParticles = nLayers * particlesPerLayer;
    soA.position.resize(totalParticles);
    soA.velocity.resize(totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    std::cout << "massValue: " << massValue << std::endl;
    soA.mass.resize(totalParticles, massValue);
    soA.type.resize(totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(totalParticles, false);
    soA.color.resize(totalParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(totalParticles, glm::dvec3(1.0));
    
    double halfWidth = (gridSize - 1) * cellSize / 2.0;
    for (int l = 0; l < nLayers; l++) {
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int idx = l * particlesPerLayer + i * gridSize + j;
                double x = j * cellSize - halfWidth;
                double z = i * cellSize - halfWidth;
                double y = l * layerSpacing;
                soA.position[idx] = glm::dvec3(x, y, z);
            }
        }
    }
    
    for (int l = 0; l < nLayers; l++) {
        for (int i = 0; i < gridSize; i++) {
            int leftIdx = l * particlesPerLayer + i * gridSize;
            int rightIdx = l * particlesPerLayer + i * gridSize + (gridSize - 1);
            soA.isStatic[leftIdx] = true;
            soA.isStatic[rightIdx] = true;
        }
    }
    
    for (int l = 0; l < nLayers; l++) {
        int layerOffset = l * particlesPerLayer;
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int idx = layerOffset + i * gridSize + j;
                if (j < gridSize - 1) {
                    int rightIdx = layerOffset + i * gridSize + (j + 1);
                    double dist = glm::distance(soA.position[idx], soA.position[rightIdx]);
                    SpringData sp;
                    sp.p1Index = idx;
                    sp.p2Index = rightIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                if (i < gridSize - 1) {
                    int bottomIdx = layerOffset + (i + 1) * gridSize + j;
                    double dist = glm::distance(soA.position[idx], soA.position[bottomIdx]);
                    SpringData sp;
                    sp.p1Index = idx;
                    sp.p2Index = bottomIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                if (i < gridSize - 1 && j < gridSize - 1) {
                    int diagIdx = layerOffset + (i + 1) * gridSize + (j + 1);
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = idx;
                    sp.p2Index = diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                if (i < gridSize - 1 && j > 0) {
                    int diagIdx = layerOffset + (i + 1) * gridSize + (j - 1);
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = idx;
                    sp.p2Index = diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
            }
        }
    }
    
    for (int l = 0; l < nLayers - 1; l++) {
        int lowerOffset = l * particlesPerLayer;
        int upperOffset = (l + 1) * particlesPerLayer;
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int lowerIdx = lowerOffset + i * gridSize + j;
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni < 0 || ni >= gridSize || nj < 0 || nj >= gridSize)
                            continue;
                        int upperIdx = upperOffset + ni * gridSize + nj;
                        double dist = glm::distance(soA.position[lowerIdx], soA.position[upperIdx]);
                        SpringData sp;
                        sp.p1Index = lowerIdx;
                        sp.p2Index = upperIdx;
                        sp.restLength = dist * springRestLength;
                        sp.springConstant = sharedParams.springConstant;
                        sp.damping = 0.5;
                        springs.push_back(sp);
                    }
                }
            }
        }
    }
}

void SimulationBase::createHexGrid(int numHexagons, double hexagonSize, double springRestLength, bool bothEndsStatic_, double orientationDegrees) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    std::vector<glm::dvec3> uniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix, uniquePositions, edgeSet);
    assignUniquePositionsToSoA(uniquePositions, bothEndsStatic_);
    createSpringsFromEdgeSet(edgeSet, springRestLength);
}

void SimulationBase::createMultiLayerHexGrid(int numHexagons, double hexagonSize, double springRestLength, bool bothEndsStatic, double orientationDegrees, double layerHeight, int nLayers) {
    clearSimulation();
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    std::vector<glm::dvec3> baseUniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix, baseUniquePositions, edgeSet);
    size_t baseCount = baseUniquePositions.size();

    glm::dvec3 minPos = baseUniquePositions[0];
    glm::dvec3 maxPos = baseUniquePositions[0];
    for (size_t i = 1; i < baseCount; i++) {
        minPos = glm::min(minPos, baseUniquePositions[i]);
        maxPos = glm::max(maxPos, baseUniquePositions[i]);
    }
    glm::dvec3 diffs = maxPos - minPos;
    glm::dvec3 verticalAxis(0.0);
    if (diffs.x <= diffs.y && diffs.x <= diffs.z)
        verticalAxis = glm::dvec3(1.0, 0.0, 0.0);
    else if (diffs.y <= diffs.x && diffs.y <= diffs.z)
        verticalAxis = glm::dvec3(0.0, 1.0, 0.0);
    else
        verticalAxis = glm::dvec3(0.0, 0.0, 1.0);
    glm::dvec3 arbitrary;
    if (fabs(verticalAxis.z) < 0.9)
        arbitrary = glm::dvec3(0.0, 0.0, 1.0);
    else
        arbitrary = glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 inPlaneAxis1 = glm::normalize(glm::cross(verticalAxis, arbitrary));
    glm::dvec3 inPlaneAxis2 = glm::normalize(glm::cross(verticalAxis, inPlaneAxis1));

    size_t totalParticles = nLayers * baseCount;
    soA.position.resize(totalParticles);
    soA.velocity.resize(totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(totalParticles, massValue);
    soA.type.resize(totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(totalParticles, false);
    soA.color.resize(totalParticles, glm::dvec3(1.0));
    soA.dimensions.resize(totalParticles, glm::dvec3(2.0));

    for (int layer = 0; layer < nLayers; layer++) {
        for (size_t i = 0; i < baseCount; i++) {
            size_t idx = layer * baseCount + i;
            glm::dvec3 pos = baseUniquePositions[i];
            pos += verticalAxis * (layerHeight * layer);
            if (layer % 2 == 1)
                pos += inPlaneAxis2 * hexagonSize;
            soA.position[idx] = pos;
        }
    }

    if (!hexagonVertexLists.empty()) {
        for (int layer = 0; layer < nLayers; layer++) {
            for (const auto &hex : hexagonVertexLists) {
                glm::dvec3 center(0.0);
                for (auto &v : hex)
                    center += v;
                center /= static_cast<double>(hex.size());
                std::vector<std::pair<double, glm::dvec3>> angleVerts;
                for (auto &v : hex) {
                    double a = atan2(v.y - center.y, v.x - center.x);
                    angleVerts.push_back({a, v});
                }
                std::sort(angleVerts.begin(), angleVerts.end(), [](const auto &A, const auto &B) {
                    return A.first > B.first;
                });
                for (size_t localIdx = 0; localIdx < angleVerts.size(); localIdx++) {
                    glm::dvec3 vertex = angleVerts[localIdx].second;
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, vertex);
                    if (baseIndex != -1) {
                        size_t globalIdx = layer * baseCount + baseIndex;
                        if (localIdx % 2 == 0)
                            soA.color[globalIdx] = glm::dvec3(1.0, 0.0, 0.0);
                        else
                            soA.color[globalIdx] = glm::dvec3(0.0, 1.0, 0.0);
                    }
                }
            }
        }
    } else {
        for (size_t i = 0; i < totalParticles; i++) {
            soA.color[i] = (i % 2 == 0) ? glm::dvec3(1.0, 0.0, 0.0)
                                          : glm::dvec3(0.0, 1.0, 0.0);
        }
    }

    glm::dvec3 globalX(1.0, 0.0, 0.0);
    glm::dvec3 horizontalAxis = globalX - (glm::dot(globalX, verticalAxis) * verticalAxis);
    double eps = 0.0001;
    if (glm::length(horizontalAxis) < eps)
        horizontalAxis = glm::dvec3(0.0, 1.0, 0.0) - (glm::dot(glm::dvec3(0.0, 1.0, 0.0), verticalAxis) * verticalAxis);
    horizontalAxis = glm::normalize(horizontalAxis);
    double edgeTolerance = 0.5;
    
    for (int layer = 0; layer < nLayers; layer++) {
        size_t layerStart = layer * baseCount;
        size_t layerEnd = layerStart + baseCount;
        double minProj = 1e9, maxProj = -1e9;
        for (size_t i = layerStart; i < layerEnd; i++) {
            glm::dvec3 pp = soA.position[i];
            double proj = glm::dot(pp, horizontalAxis);
            if (proj < minProj) minProj = proj;
            if (proj > maxProj) maxProj = proj;
        }
        for (size_t i = layerStart; i < layerEnd; i++) {
            glm::dvec3 pp = soA.position[i];
            double proj = glm::dot(pp, horizontalAxis);
            if (fabs(proj - minProj) < edgeTolerance)
                soA.isStatic[i] = true;
            if (bothEndsStatic && fabs(proj - maxProj) < edgeTolerance)
                soA.isStatic[i] = true;
        }
    }
    
    for (int layer = 0; layer < nLayers; layer++) {
        for (const auto &edge : edgeSet) {
            int i1 = layer * baseCount + edge.first;
            int i2 = layer * baseCount + edge.second;
            double dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = sharedParams.springConstant;
            sp.damping = 0.5;
            springs.push_back(sp);
        }
    }
}

glm::dmat4 SimulationBase::createRotationMatrix(double orientationDegrees) {
    return glm::rotate(glm::dmat4(1.0), glm::radians(orientationDegrees), glm::dvec3(1.0, 0.0, 0.0));
}

void SimulationBase::generateHexagonCells(int numHexagons, double hexagonSize, const glm::dmat4& rotationMatrix,
                                      std::vector<glm::dvec3>& uniquePositions,
                                      std::set<std::pair<int,int>>& edgeSet) {
    auto findOrAdd = [&](const glm::dvec3 &pos) -> int {
        double eps = 0.0001;
        for (int i = 0; i < (int)uniquePositions.size(); i++) {
            glm::dvec3 diff = uniquePositions[i] - pos;
            if (glm::length(diff) < eps) {
                return i;
            }
        }
        uniquePositions.push_back(pos);
        return (int)uniquePositions.size() - 1;
    };

    for (int r = 0; r < numHexagons; r++) {
        for (int c = 0; c < numHexagons; c++) {
            glm::dvec3 center;
            center.x = sqrt(3.0) * hexagonSize * (c + (r % 2) * 0.5);
            center.y = 1.5 * hexagonSize * r;
            center.z = 0.0;
            center = glm::dvec3(rotationMatrix * glm::dvec4(center, 1.0));
            std::vector<glm::dvec3> currVerts;
            std::vector<int> indices;
            currVerts.reserve(6);
            indices.reserve(6);
            for (int i = 0; i < 6; i++) {
                double ang = glm::radians(60.0 * i + 90.0);
                glm::dvec3 offset(
                    hexagonSize * cos(ang),
                    hexagonSize * sin(ang),
                    0.0
                );
                offset = glm::dvec3(rotationMatrix * glm::dvec4(offset, 0.0));
                glm::dvec3 vertex = center + offset;
                currVerts.push_back(vertex);
                indices.push_back(findOrAdd(vertex));
            }
            hexagonVertexLists.push_back(currVerts);
            hexagonIndices.push_back(indices);
            for (int i = 0; i < 6; i++) {
                int idx1 = indices[i];
                int idx2 = indices[(i + 1) % 6];
                if (idx1 > idx2) std::swap(idx1, idx2);
                edgeSet.insert({ idx1, idx2 });
            }
        }
    }
}

void SimulationBase::assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions, bool bothEndsStatic) {
    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::dvec3(0.0));
    soA.forceAccum.resize(n, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(n, massValue);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::dvec3(1.0, 0.4, 0.0));
    soA.dimensions.resize(n, glm::dvec3(1.0));

    double minX = 1e9, maxX = -1e9;
    for (auto &p : uniquePositions) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    for (size_t i = 0; i < n; i++) {
        soA.position[i] = uniquePositions[i];
    }
    for (size_t i = 0; i < n; i++) {
        double px = soA.position[i].x;
        bool leftStatic = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }
}

void SimulationBase::createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, double springRestLength) {
    for (auto &e : edgeSet) {
        int i1 = e.first;
        int i2 = e.second;
        double dist = glm::distance(soA.position[i1], soA.position[i2]);
        SpringData sp;
        sp.p1Index = i1;
        sp.p2Index = i2;
        sp.restLength = dist * springRestLength;
        sp.springConstant = sharedParams.springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

bool SimulationBase::approxEqualVec3(const glm::dvec3& a, const glm::dvec3& b, double epsilon) {
    return (fabs(a.x - b.x) < epsilon) &&
           (fabs(a.y - b.y) < epsilon) &&
           (fabs(a.z - b.z) < epsilon);
}

int SimulationBase::findApproxVertexIndex(const std::vector<glm::dvec3>& vertices, const glm::dvec3& target, double epsilon) {
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (approxEqualVec3(vertices[i], target, epsilon)) {
            return static_cast<int>(i);
        }
    }
    return -1;
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