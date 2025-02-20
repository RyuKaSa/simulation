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
// ThreadPool
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
// Simulation
// --------------------
Simulation::Simulation() {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 2;
    }
    threadPool = new ThreadPool(numThreads);
    std::cout << "Number of threads: " << numThreads << std::endl;

    soA.position.reserve(100000);
    soA.velocity.reserve(100000);
    soA.forceAccum.reserve(100000);
    soA.mass.reserve(100000);
    soA.type.reserve(100000);
    soA.isStatic.reserve(100000);
    soA.color.reserve(100000);
    soA.dimensions.reserve(100000);

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
    // Just an example usage
    createMultiLayerHexGrid(50, 1.0, 1.0, true, 90.0, 1.0, 3);

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
    for (size_t i=0; i<soA.isStatic.size(); i++) {
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
    size_t chunkSize = (totalSprings + numThreads - 1)/numThreads;
    std::vector<std::future<void>> futures;

    for (auto &f : soA.forceAccum) {
        f = glm::dvec3(0.0);
    }

    // 1) Spring force
    for (unsigned int t=0; t<numThreads; t++) {
        size_t start = t*chunkSize;
        size_t end   = std::min(start+chunkSize, totalSprings);
        futures.push_back(
            threadPool->enqueue([this, start, end]() {
                for (size_t i=start; i<end; i++) {
                    auto &sp = springs[i];
                    int i1 = sp.p1Index;
                    int i2 = sp.p2Index;
                    glm::dvec3 pos1 = soA.position[i1];
                    glm::dvec3 pos2 = soA.position[i2];
                    glm::dvec3 vel1 = soA.velocity[i1];
                    glm::dvec3 vel2 = soA.velocity[i2];
                    double dist = glm::distance(pos1, pos2);
                    if (dist < 1e-7) continue;

                    glm::dvec3 dir = (pos1 - pos2)/dist;
                    double k  = sp.springConstant;
                    double z  = sp.damping;
                    double rest = sp.restLength;
                    double m_eff = (soA.mass[i1] + soA.mass[i2])*0.5;
                    double adjustedDamping = 2.0 * z * sqrt(m_eff*k);
                    glm::dvec3 dampingForce = adjustedDamping*(vel2 - vel1);

                    // Just a normal Hookean spring:
                    glm::dvec3 springForce = -k*(dist - rest)*dir;
                    glm::dvec3 totalForce  = springForce + dampingForce;
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
    size_t chunkPart = (n + numThreads -1)/numThreads;
    for (unsigned int t=0; t<numThreads; t++) {
        size_t start = t*chunkPart;
        size_t end   = std::min(start+chunkPart, n);
        futures.push_back(
            threadPool->enqueue([this, dt, start, end]() {
                #pragma omp simd
                for (size_t i=start; i<end; i++) {
                    if (soA.isStatic[i]) {
                        soA.forceAccum[i] = glm::dvec3(0.0);
                    } else {
                        double m = soA.mass[i];
                        glm::dvec3 accel = soA.forceAccum[i]/m;
                        soA.velocity[i] += accel*dt;
                        soA.position[i] += soA.velocity[i]*dt;
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
    for (size_t cubeIndex=0; cubeIndex<soA.position.size(); cubeIndex++) {
        if (soA.type[cubeIndex] != ParticleType::EXTERNAL) {
            continue;
        }
        glm::dvec3 cubeCenter = soA.position[cubeIndex];
        glm::dvec3 halfExtents = soA.dimensions[cubeIndex]*0.5;

        for (size_t i=0; i<soA.position.size(); i++) {
            if (soA.type[i] != ParticleType::STRUCTURE) continue;
            if (soA.isStatic[i]) continue;
            glm::dvec3 pos = soA.position[i];
            glm::dvec3 closest;
            for (int j=0; j<3; j++) {
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
                    normal = diff/dist;
                } else {
                    normal = glm::dvec3(0,1,0);
                }
                soA.position[i] += normal*penetration;
                double vn = glm::dot(soA.velocity[i], normal);
                if (vn < 0) {
                    soA.velocity[i] -= (1.0+restitution)*vn*normal;
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

void Simulation::addStaticCubeUnderGrid() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    glm::dvec3 center(0.0);
    int count = 0;
    for (size_t i=0; i<soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            center += soA.position[i];
            count++;
        }
    }
    if (count>0) {
        center /= (double)count;
    } else {
        center = glm::dvec3(0.0);
    }
    double cubeSize = 80.0;
    glm::dvec3 cubePos = center + glm::dvec3(0.0, -cubeSize-0.5, 0.0);

    soA.position.push_back(cubePos);
    soA.velocity.push_back(glm::dvec3(0.0));
    soA.forceAccum.push_back(glm::dvec3(0.0));
    soA.mass.push_back(100.0);
    soA.type.push_back(ParticleType::EXTERNAL);
    soA.isStatic.push_back(true);
    soA.color.push_back(glm::dvec3(0.56,1.0,0.4));
    soA.dimensions.push_back(glm::dvec3(cubeSize));
}

void Simulation::createCord(int numBalls,
                            double length,
                            double springRestLength,
                            bool bothEndsStatic_)
{
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

    double spacing = length/(numBalls-1);
    glm::dvec3 startPos(-length/2, 0.0, 0.0);

    for (int i=0; i<numBalls; i++) {
        glm::dvec3 pos = startPos + glm::dvec3(i*spacing, 0.0, 0.0);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::dvec3(0.0));
        soA.forceAccum.push_back(glm::dvec3(0.0));
        soA.mass.push_back(10.0);
        soA.type.push_back(ParticleType::STRUCTURE);
        bool sflag = (bothEndsStatic_)? (i==0 || i==(numBalls-1)) : (i==0);
        soA.isStatic.push_back(sflag);
        soA.color.push_back(glm::dvec3(1,0,0));
        soA.dimensions.push_back(glm::dvec3(3.0));
    }
    for (int i=0; i<numBalls-1; i++) {
        double dist = glm::distance(soA.position[i], soA.position[i+1]);
        SpringData sp;
        sp.p1Index = i;
        sp.p2Index = i+1;
        sp.restLength = dist*springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

void Simulation::createHexGrid(int numHexagons,
                               double hexagonSize,
                               double springRestLength,
                               bool bothEndsStatic_,
                               double orientationDegrees)
{
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    // Use double-based rotation matrix:
    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    std::vector<glm::dvec3> uniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix,
                         uniquePositions, edgeSet);
    assignUniquePositionsToSoA(uniquePositions, bothEndsStatic_);
    createSpringsFromEdgeSet(edgeSet, springRestLength);
    // updateHexTriangles();
}

void Simulation::createMultiLayerHexGrid(int numHexagons,
                                           double hexagonSize,
                                           double springRestLength,
                                           bool bothEndsStatic,
                                           double orientationDegrees,
                                           double layerHeight,
                                           int nLayers)
{
    clearSimulation();
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    std::vector<glm::dvec3> baseUniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix,
                         baseUniquePositions, edgeSet);
    size_t baseCount = baseUniquePositions.size();

    glm::dvec3 minPos = baseUniquePositions[0];
    glm::dvec3 maxPos = baseUniquePositions[0];
    for (size_t i = 1; i < baseCount; i++) {
        minPos = glm::min(minPos, baseUniquePositions[i]);
        maxPos = glm::max(maxPos, baseUniquePositions[i]);
    }
    glm::dvec3 diffs = maxPos - minPos;
    glm::dvec3 verticalAxis(0.0);
    if (diffs.x <= diffs.y && diffs.x <= diffs.z) {
        verticalAxis = glm::dvec3(1.0, 0.0, 0.0);
    } else if (diffs.y <= diffs.x && diffs.y <= diffs.z) {
        verticalAxis = glm::dvec3(0.0, 1.0, 0.0);
    } else {
        verticalAxis = glm::dvec3(0.0, 0.0, 1.0);
    }
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
    soA.mass.resize(totalParticles, 1.0);
    soA.type.resize(totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(totalParticles, false);
    soA.color.resize(totalParticles, glm::dvec3(1.0));
    soA.dimensions.resize(totalParticles, glm::dvec3(2.0));

    // --- Create Layers (Assign Positions) ---
    for (int layer = 0; layer < nLayers; layer++) {
        for (size_t i = 0; i < baseCount; i++) {
            size_t idx = layer * baseCount + i;
            glm::dvec3 pos = baseUniquePositions[i];
            pos += verticalAxis * (layerHeight * layer);
            if (layer % 2 == 1) {
                pos += inPlaneAxis2 * hexagonSize;
            }
            soA.position[idx] = pos;
        }
    }

    // --- Assign Colors Per Hex Cell ---
    if (!hexagonVertexLists.empty()) {
        for (int layer = 0; layer < nLayers; layer++) {
            for (const auto &hex : hexagonVertexLists) {
                glm::dvec3 center(0.0);
                for (auto &v : hex) {
                    center += v;
                }
                center /= static_cast<double>(hex.size());
                std::vector<std::pair<double, glm::dvec3>> angleVerts;
                for (auto &v : hex) {
                    double a = atan2(v.y - center.y, v.x - center.x);
                    angleVerts.push_back({a, v});
                }
                std::sort(angleVerts.begin(), angleVerts.end(),
                          [](const auto &A, const auto &B) {
                              return A.first > B.first;
                          });
                for (size_t localIdx = 0; localIdx < angleVerts.size(); localIdx++) {
                    glm::dvec3 vertex = angleVerts[localIdx].second;
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, vertex);
                    if (baseIndex != -1) {
                        size_t globalIdx = layer * baseCount + baseIndex;
                        if (localIdx % 2 == 0) {
                            soA.color[globalIdx] = glm::dvec3(1.0, 0.0, 0.0);
                        } else {
                            soA.color[globalIdx] = glm::dvec3(0.0, 1.0, 0.0);
                        }
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

    // --- Set Static Particles Along the Left and (optionally) Right Edges ---
    glm::dvec3 globalX(1.0, 0.0, 0.0);
    glm::dvec3 horizontalAxis = globalX - (glm::dot(globalX, verticalAxis) * verticalAxis);
    double eps = 0.0001;
    if (glm::length(horizontalAxis) < eps) {
        horizontalAxis = glm::dvec3(0.0, 1.0, 0.0) -
                         (glm::dot(glm::dvec3(0.0, 1.0, 0.0), verticalAxis) * verticalAxis);
    }
    horizontalAxis = glm::normalize(horizontalAxis);
    double edgeTolerance = 0.5;

    for (int layer = 0; layer < nLayers; layer++) {
        size_t layerStart = layer * baseCount;
        size_t layerEnd   = layerStart + baseCount;
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
            if (fabs(proj - minProj) < edgeTolerance) {
                soA.isStatic[i] = true;
            }
            if (bothEndsStatic && fabs(proj - maxProj) < edgeTolerance) {
                soA.isStatic[i] = true;
            }
        }
    }

    // --- Intra-layer Springs ---
    for (int layer = 0; layer < nLayers; layer++) {
        for (const auto &edge : edgeSet) {
            int i1 = layer * baseCount + edge.first;
            int i2 = layer * baseCount + edge.second;
            double dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5;
            springs.push_back(sp);
        }
    }

    // --- Inter-layer Springs (Using Float Precision) ---
    if (!hexagonVertexLists.empty()) {
        for (int layer = 0; layer < nLayers - 1; layer++) {
            for (const auto &hex : hexagonVertexLists) {
                std::vector<size_t> lowerGreenIndices;
                std::vector<glm::vec3> lowerGreenPositions;
                std::vector<size_t> upperRedIndices;
                std::vector<glm::vec3> upperRedPositions;
                
                for (const glm::dvec3 &baseVertex : hex) {
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, baseVertex);
                    if (baseIndex == -1)
                        continue;
                    size_t lowerGlobalIndex = layer * baseCount + baseIndex;
                    size_t upperGlobalIndex = (layer + 1) * baseCount + baseIndex;
                    
                    // Convert color to float and compare as in Code 1.
                    if (approxEqualVec3(glm::vec3(soA.color[lowerGlobalIndex]),
                                        glm::vec3(1.0f, 0.0f, 0.0f))) {
                        lowerGreenIndices.push_back(lowerGlobalIndex);
                        lowerGreenPositions.push_back(glm::vec3(soA.position[lowerGlobalIndex]));
                    }
                    if (approxEqualVec3(glm::vec3(soA.color[upperGlobalIndex]),
                                        glm::vec3(0.0f, 1.0f, 0.0f))) {
                        upperRedIndices.push_back(upperGlobalIndex);
                        upperRedPositions.push_back(glm::vec3(soA.position[upperGlobalIndex]));
                    }
                }
                
                // --- Bottom-up pass ---
                if (!lowerGreenPositions.empty() && !upperRedPositions.empty()) {
                    glm::vec3 avgLowerGreen(0.0f);
                    for (const auto &p : lowerGreenPositions)
                        avgLowerGreen += p;
                    avgLowerGreen /= static_cast<float>(lowerGreenPositions.size());
                    
                    size_t chosenUpperRed = 0;
                    float bestDistance = std::numeric_limits<float>::max();
                    for (size_t i = 0; i < upperRedPositions.size(); i++) {
                        float d = glm::distance(upperRedPositions[i], avgLowerGreen);
                        if (d < bestDistance) {
                            bestDistance = d;
                            chosenUpperRed = upperRedIndices[i];
                        }
                    }
                    
                    for (size_t i = 0; i < lowerGreenIndices.size(); i++) {
                        SpringData sp;
                        sp.p1Index = lowerGreenIndices[i];
                        sp.p2Index = chosenUpperRed;
                        float dist = glm::distance(glm::vec3(soA.position[sp.p1Index]),
                                                   glm::vec3(soA.position[sp.p2Index]));
                        sp.restLength = dist * static_cast<float>(springRestLength);
                        sp.springConstant = springConstant;
                        sp.damping = 0.5f;
                        springs.push_back(sp);
                    }
                }
                
                // --- Top-down pass ---
                if (!lowerGreenPositions.empty() && !upperRedPositions.empty()) {
                    glm::vec3 avgUpperRed(0.0f);
                    for (const auto &p : upperRedPositions)
                        avgUpperRed += p;
                    avgUpperRed /= static_cast<float>(upperRedPositions.size());
                    
                    size_t chosenLowerGreen = 0;
                    float bestDistance = std::numeric_limits<float>::max();
                    for (size_t i = 0; i < lowerGreenPositions.size(); i++) {
                        float d = glm::distance(lowerGreenPositions[i], avgUpperRed);
                        if (d < bestDistance) {
                            bestDistance = d;
                            chosenLowerGreen = lowerGreenIndices[i];
                        }
                    }
                    
                    for (size_t i = 0; i < upperRedIndices.size(); i++) {
                        SpringData sp;
                        sp.p1Index = chosenLowerGreen;
                        sp.p2Index = upperRedIndices[i];
                        float dist = glm::distance(glm::vec3(soA.position[sp.p1Index]),
                                                   glm::vec3(soA.position[sp.p2Index]));
                        sp.restLength = dist * static_cast<float>(springRestLength);
                        sp.springConstant = springConstant;
                        sp.damping = 0.5f;
                        springs.push_back(sp);
                    }
                }
            }
        }
    }
}

bool Simulation::approxEqualVec3(const glm::dvec3& a, const glm::dvec3& b, double epsilon) {
    return (fabs(a.x - b.x) < epsilon) &&
           (fabs(a.y - b.y) < epsilon) &&
           (fabs(a.z - b.z) < epsilon);
}

int Simulation::findApproxVertexIndex(const std::vector<glm::dvec3>& vertices, const glm::dvec3& target, double epsilon) {
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (approxEqualVec3(vertices[i], target, epsilon)) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found.
}

glm::dmat4 Simulation::createRotationMatrix(double orientationDegrees) {
    return glm::rotate(glm::dmat4(1.0), glm::radians(orientationDegrees), glm::dvec3(1.0, 0.0, 0.0));
}

void Simulation::generateHexagonCells(int numHexagons,
                                      double hexagonSize,
                                      const glm::dmat4& rotationMatrix,
                                      std::vector<glm::dvec3>& uniquePositions,
                                      std::set<std::pair<int,int>>& edgeSet)
{
    auto findOrAdd = [&](const glm::dvec3 &pos) -> int {
        double eps = 0.0001;
        for (int i = 0; i < (int)uniquePositions.size(); i++) {
            glm::dvec3 diff = uniquePositions[i] - pos;
            if (glm::length(diff) < eps) {
                return i;
            }
        }
        uniquePositions.push_back(pos);
        return (int)uniquePositions.size()-1;
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

void Simulation::assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions,
                                              bool bothEndsStatic)
{
    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::dvec3(0.0));
    soA.forceAccum.resize(n, glm::dvec3(0.0));
    soA.mass.resize(n, 10.0);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::dvec3(1.0,0.4,0.0));
    soA.dimensions.resize(n, glm::dvec3(1.0));

    double minX = 1e9, maxX = -1e9;
    for (auto &p : uniquePositions) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    for (size_t i = 0; i < n; i++) {
        // Use the proper type here:
        const glm::dvec3 &v = uniquePositions[i];
        soA.position[i] = v;
    }
    for (size_t i = 0; i < n; i++) {
        double px = soA.position[i].x;
        bool leftStatic  = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }
}

void Simulation::createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet,
                                          double springRestLength)
{
    for (auto &e : edgeSet) {
        int i1 = e.first;
        int i2 = e.second;
        double dist = glm::distance(soA.position[i1], soA.position[i2]);
        SpringData sp;
        sp.p1Index = i1;
        sp.p2Index = i2;
        sp.restLength = dist*springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

void Simulation::updateHexTriangles() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    hexTriangles.clear();
    size_t totalHexes = hexagonIndices.size();
    if (totalHexes==0) return;

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads==0) numThreads=2;
    size_t chunkSize = (totalHexes+ numThreads -1)/numThreads;

    std::vector<std::future<std::vector<HexTriangle>>> futures;
    futures.reserve(numThreads);

    for (unsigned int t=0; t<numThreads; t++) {
        size_t start = t*chunkSize;
        size_t end   = std::min(start+chunkSize, totalHexes);
        if (start>=end) break;

        futures.push_back(
            threadPool->enqueue([this,start,end](){
                std::vector<HexTriangle> localTris;
                localTris.reserve((end-start)*6);
                for (size_t h=start; h<end; h++) {
                    const std::vector<int> &inds = hexagonIndices[h];
                    glm::dvec3 corners[6];
                    for (int i=0; i<6; i++) {
                        corners[i] = soA.position[inds[i]];
                    }
                    glm::dvec3 center(0.0);
                    for (int i=0; i<6; i++) {
                        center+= corners[i];
                    }
                    center/= 6.0;
                    for (int i=0; i<6; i++) {
                        HexTriangle tri;
                        // We store them as float-based in the struct
                        glm::dvec3 v0 = center;
                        glm::dvec3 v1 = corners[i];
                        glm::dvec3 v2 = corners[(i+1)%6];
                        tri.vertices[0] = glm::vec3((float)v0.x,(float)v0.y,(float)v0.z);
                        tri.vertices[1] = glm::vec3((float)v1.x,(float)v1.y,(float)v1.z);
                        tri.vertices[2] = glm::vec3((float)v2.x,(float)v2.y,(float)v2.z);

                        glm::dvec3 edge1 = v1-v0;
                        glm::dvec3 edge2 = v2-v0;
                        glm::dvec3 n = glm::normalize(glm::cross(edge1, edge2));
                        tri.normal = glm::vec3((float)n.x,(float)n.y,(float)n.z);
                        localTris.push_back(tri);
                    }
                }
                return localTris;
            })
        );
    }
    size_t totalTriCount = 0;
    std::vector<std::vector<HexTriangle>> partials(numThreads);
    for (size_t i=0; i<futures.size(); i++) {
        std::vector<HexTriangle> out = futures[i].get();
        totalTriCount+= out.size();
        partials[i] = std::move(out);
    }
    hexTriangles.reserve(totalTriCount);
    for (auto &p : partials) {
        hexTriangles.insert(hexTriangles.end(), p.begin(), p.end());
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
        if ((now - lastMeasure)>=1.0) {
            effectiveStepsPerSecond.store(stepsCount);
            stepsCount=0;
            lastMeasure=now;
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
    return soA;  // This creates a COPY
}

const std::vector<HexTriangle>& Simulation::getHexTriangles() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return hexTriangles;
}

std::vector<glm::dvec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::dvec3> ret;
    ret.reserve(soA.position.size());
    for (size_t i=0; i<soA.position.size(); i++) {
        if (soA.type[i]==ParticleType::STRUCTURE) {
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