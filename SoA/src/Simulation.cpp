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

// ====================
// THREAD POOL CLASS (Nested within Simulation)
// ====================
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
        for (auto &worker : workers)
            worker.join();
    }
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// ====================
// SIMULATION IMPLEMENTATION
// ====================

Simulation::Simulation() {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 2;
    threadPool = new ThreadPool(numThreads);
    // print number of threads
    std::cout << "Number of threads: " << numThreads << std::endl;

    // Pre-allocate memory for particle data (SoA)
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
    // createHexGrid(60, 1.3f, 1.0f, true, 90.0f);
    createMultiLayerHexGrid(60, 1.0f, 1.0f, true, 90.0f, 1.0f, 5.0f);
    // Gravity link
    if (gravityLink) { delete gravityLink; }
    gravityLink = new Link(soA, glm::vec3(0.0f, -9.81f, 0.0f));
    addStaticCubeUnderGrid();
}

void Simulation::reset() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "RESET initialized" << std::endl;
    update(0.0f);
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

void Simulation::setSpringConstant(float k) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    springConstant = k;
    for (auto &sp : springs) {
        sp.springConstant = k;
    }
}

void Simulation::setDampingCoefficient(float z) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    for (auto &sp : springs) {
        sp.damping = z;
    }
}

// New method: Drop Structure
void Simulation::dropStructure() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "Dropping structure: converting static particles to dynamic." << std::endl;
    for (size_t i = 0; i < soA.isStatic.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE && soA.isStatic[i]) {
            soA.isStatic[i] = false;
        }
    }
}

void Simulation::update(float dt) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 2;
    
    // --- 1) Parallel Spring Update ---
    size_t totalSprings = springs.size();
    size_t chunkSize = (totalSprings + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;
    
    for (auto &f : soA.forceAccum)
        f = glm::vec3(0.0f);
    
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end   = std::min(start + chunkSize, totalSprings);
        futures.push_back(
            threadPool->enqueue([this, start, end]() {
                for (size_t i = start; i < end; i++) {
                    auto &sp = springs[i];
                    int i1 = sp.p1Index;
                    int i2 = sp.p2Index;
                    
                    glm::vec3 pos1 = soA.position[i1];
                    glm::vec3 pos2 = soA.position[i2];
                    glm::vec3 vel1 = soA.velocity[i1];
                    glm::vec3 vel2 = soA.velocity[i2];
                    float dist = glm::distance(pos1, pos2);
                    if (dist < 1e-7f)
                        continue;
                    
                    glm::vec3 dir = (pos1 - pos2) / dist;
                    float k    = sp.springConstant;
                    float z    = sp.damping;
                    float rest = sp.restLength;
                    
                    float maxLength = 1.1f * rest;
                    float m_eff = (soA.mass[i1] + soA.mass[i2]) * 0.5f;
                    float adjustedDamping = 2.0f * z * sqrt(m_eff * k);
                    glm::vec3 dampingForce = adjustedDamping * (vel2 - vel1);
                    
                    glm::vec3 totalForce;
                    if (dist > maxLength) {
                        float extraStretch = dist - maxLength;
                        float k_extra = k * 100.0f;
                        glm::vec3 normalForce = -k * (maxLength - rest) * dir;
                        glm::vec3 extraForce  = -k_extra * extraStretch * dir;
                        totalForce = normalForce + extraForce + dampingForce;
                    } else {
                        totalForce = -k * (dist - rest) * dir + dampingForce;
                    }
                    soA.forceAccum[i1] += totalForce;
                    soA.forceAccum[i2] -= totalForce;
                }
            })
        );
    }
    for (auto &f : futures)
        f.get();
    futures.clear();
    
    // --- 2) Gravity Link Update (Serial) ---
    if (gravityLink) {
        applyGravityLink();
    }
    
    // --- 3) Parallel Particle Integration with SIMD Hint ---
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
                        soA.forceAccum[i] = glm::vec3(0.0f);
                    } else {
                        float m = soA.mass[i];
                        glm::vec3 accel = soA.forceAccum[i] / m;
                        soA.velocity[i] += accel * dt;
                        soA.position[i] += soA.velocity[i] * dt;
                        soA.forceAccum[i] = glm::vec3(0.0f);
                    }
                }
            })
        );
    }
    for (auto &f : futures)
        f.get();
    futures.clear();
    
    // --- 4) Particle-Cube Collision Resolution ---
    // Now resolve collisions between dynamic structure particles and all external (cube) particles.
    resolveExternalCollisions();
}

void Simulation::resolveExternalCollisions() {
    // Constants for collision resolution
    const float restitution = 0.5f;
    const float particleRadius = 0.1f;

    // Loop over all external particles (cubes)
    for (size_t cubeIndex = 0; cubeIndex < soA.position.size(); cubeIndex++) {
        if (soA.type[cubeIndex] != ParticleType::EXTERNAL)
            continue;
        // Get cube data (assumed to be axis-aligned)
        glm::vec3 cubeCenter = soA.position[cubeIndex];
        glm::vec3 halfExtents = soA.dimensions[cubeIndex] * 0.5f;

        // Check collision for each dynamic structure particle
        for (size_t i = 0; i < soA.position.size(); i++) {
            if (soA.type[i] != ParticleType::STRUCTURE)
                continue;
            if (soA.isStatic[i])
                continue;

            glm::vec3 pos = soA.position[i];
            // Compute the closest point on the cube's AABB to the particle position
            glm::vec3 closest;
            for (int j = 0; j < 3; j++) {
                float cubeMin = cubeCenter[j] - halfExtents[j];
                float cubeMax = cubeCenter[j] + halfExtents[j];
                if (pos[j] < cubeMin)
                    closest[j] = cubeMin;
                else if (pos[j] > cubeMax)
                    closest[j] = cubeMax;
                else
                    closest[j] = pos[j];
            }
            glm::vec3 diff = pos - closest;
            float dist = glm::length(diff);
            // If penetration occurs, resolve collision
            if (dist < particleRadius) {
                float penetration = particleRadius - dist;
                glm::vec3 normal;
                if (dist > 1e-6f) {
                    normal = diff / dist;
                } else {
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                soA.position[i] += normal * penetration;
                float vn = glm::dot(soA.velocity[i], normal);
                if (vn < 0) {
                    soA.velocity[i] -= (1.0f + restitution) * vn * normal;
                }
            }
        }
    }
}

void Simulation::applyGravityLink() {
    if (gravityLink)
        gravityLink->applyGravity();
}

void Simulation::addStaticCubeUnderGrid() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    
    // Compute the average center of all structure particles.
    glm::vec3 center(0.0f);
    int count = 0;
    for (size_t i = 0; i < soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            center += soA.position[i];
            count++;
        }
    }
    if (count > 0)
        center /= static_cast<float>(count);
    else
        center = glm::vec3(0.0f);  // Fallback
    
    // Define the cube's size and position it just under the structure.
    const float cubeSize = 80.0f;  // Adjust as needed
    glm::vec3 cubePos = center + glm::vec3(0.0f, -cubeSize - 0.5f, 0.0f);
    
    // Add a new particle representing the cube.
    soA.position.push_back(cubePos);
    soA.velocity.push_back(glm::vec3(0.0f));
    soA.forceAccum.push_back(glm::vec3(0.0f));
    soA.mass.push_back(1.0f);  // Mass is irrelevant for static particles.
    soA.type.push_back(ParticleType::EXTERNAL);
    soA.isStatic.push_back(true);
    
    // set color and dimensions
    soA.color.push_back(glm::vec3(0.56f, 1.0f, 0.4f));
    soA.dimensions.push_back(glm::vec3(cubeSize));
}

void Simulation::createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic_) {
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
    
    float spacing = length / (numBalls - 1);
    glm::vec3 startPos(-length/2, 0.f, 0.f);
    
    for (int i = 0; i < numBalls; i++) {
        glm::vec3 pos = startPos + glm::vec3(i * spacing, 0.f, 0.f);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::vec3(0));
        soA.forceAccum.push_back(glm::vec3(0));
        soA.mass.push_back(10.f);
        soA.type.push_back(ParticleType::STRUCTURE);
        bool sflag = (bothEndsStatic_) ? (i == 0 || i == (numBalls - 1)) : (i == 0);
        soA.isStatic.push_back(sflag);
        soA.color.push_back(glm::vec3(1, 0, 0));
        soA.dimensions.push_back(glm::vec3(3.f));
    }
    
    for (int i = 0; i < numBalls - 1; i++) {
        float dist = glm::distance(soA.position[i], soA.position[i+1]);
        SpringData sp;
        sp.p1Index = i;
        sp.p2Index = i + 1;
        sp.restLength = dist * springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5f;
        springs.push_back(sp);
    }
}

void Simulation::createHexGrid(int numHexagons, float hexagonSize, float springRestLength, bool bothEndsStatic_, float orientationDegrees) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;

    // Clear any previous data
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    // Create a rotation matrix for the given orientation
    glm::mat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    // Containers to hold unique positions and edge data.
    std::vector<glm::vec3> uniquePositions;
    std::set<std::pair<int,int>> edgeSet;

    // Generate hexagon cells (populates hexagonVertexLists, hexagonIndices,
    // uniquePositions, and edgeSet)
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix, uniquePositions, edgeSet);

    // Assign unique positions to the SoA (and mark boundary particles as static)
    assignUniquePositionsToSoA(uniquePositions, bothEndsStatic_);

    // Create springs for every unique edge.
    createSpringsFromEdgeSet(edgeSet, springRestLength);

    // updateHexTriangles();
}

// Helper function to compare two glm::vec3 values approximately.
bool approxEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
    return (fabs(a.x - b.x) < epsilon) &&
           (fabs(a.y - b.y) < epsilon) &&
           (fabs(a.z - b.z) < epsilon);
}

// Helper function to find an index in a vector of glm::vec3 that is approximately equal to a target.
int findApproxVertexIndex(const std::vector<glm::vec3>& vertices, const glm::vec3& target, float epsilon = 0.001f) {
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (approxEqualVec3(vertices[i], target, epsilon)) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found.
}

void Simulation::createMultiLayerHexGrid(int numHexagons, float hexagonSize, float springRestLength,
                                           bool bothEndsStatic, float orientationDegrees, float layerHeight,
                                           int nLayers)
{
    // Clear previous simulation data.
    clearSimulation();
    springs.clear();
    hexagonVertexLists.clear();  // Should be populated by your hexagon generation process.
    hexagonIndices.clear();
    hexTriangles.clear();

    // Create a rotation matrix (used in the base hex grid generation).
    glm::mat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    // Generate base-layer hex grid data: unique positions and edge set.
    std::vector<glm::vec3> baseUniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix, baseUniquePositions, edgeSet);
    size_t baseCount = baseUniquePositions.size();

    // --- Determine the Vertical (Out-of-Plane) Axis ---
    glm::vec3 minPos = baseUniquePositions[0];
    glm::vec3 maxPos = baseUniquePositions[0];
    for (size_t i = 1; i < baseCount; i++) {
        minPos = glm::min(minPos, baseUniquePositions[i]);
        maxPos = glm::max(maxPos, baseUniquePositions[i]);
    }
    glm::vec3 diffs = maxPos - minPos;
    glm::vec3 verticalAxis(0.0f);
    if (diffs.x <= diffs.y && diffs.x <= diffs.z) {
        verticalAxis = glm::vec3(1, 0, 0); // Grid primarily spans y and z.
    } else if (diffs.y <= diffs.x && diffs.y <= diffs.z) {
        verticalAxis = glm::vec3(0, 1, 0); // Grid primarily spans x and z.
    } else {
        verticalAxis = glm::vec3(0, 0, 1); // Grid primarily spans x and y.
    }
    
    // --- Compute the In-Plane Axes ---
    glm::vec3 arbitrary;
    if (fabs(verticalAxis.z) < 0.9f) {
        arbitrary = glm::vec3(0, 0, 1);
    } else {
        arbitrary = glm::vec3(0, 1, 0);
    }
    glm::vec3 inPlaneAxis1 = glm::normalize(glm::cross(verticalAxis, arbitrary));
    glm::vec3 inPlaneAxis2 = glm::normalize(glm::cross(verticalAxis, inPlaneAxis1));

    // --- Resize SoA Arrays for All Layers ---
    size_t totalParticles = nLayers * baseCount;
    soA.position.resize(totalParticles);
    soA.velocity.resize(totalParticles, glm::vec3(0));
    soA.forceAccum.resize(totalParticles, glm::vec3(0));
    soA.mass.resize(totalParticles, 10.f);
    soA.type.resize(totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(totalParticles, false); // Start with all particles non-static.
    soA.color.resize(totalParticles); // Colors will be set per hex.
    soA.dimensions.resize(totalParticles, glm::vec3(2.0f));

    // --- Create Layers (Assign Positions) ---
    for (int layer = 0; layer < nLayers; layer++) {
        for (size_t i = 0; i < baseCount; i++) {
            size_t idx = layer * baseCount + i;
            glm::vec3 pos = baseUniquePositions[i];
            // Apply vertical offset.
            pos += verticalAxis * (layerHeight * layer);
            // Apply in-plane offset for odd layers.
            if (layer % 2 == 1) {
                pos += inPlaneAxis2 * hexagonSize;
            }
            soA.position[idx] = pos;
            // Set a default color.
            soA.color[idx] = glm::vec3(1.0f);
        }
    }

    // --- Assign Colors Per Hex Cell ---
    if (!hexagonVertexLists.empty())
    {
        for (int layer = 0; layer < nLayers; layer++) {
            for (const auto &hex : hexagonVertexLists) {
                // Compute the center of the hex.
                glm::vec3 center(0.0f);
                for (const auto &vertex : hex) {
                    center += vertex;
                }
                center /= static_cast<float>(hex.size());

                // Create a vector of (angle, vertex) pairs.
                std::vector<std::pair<float, glm::vec3>> angleVertices;
                for (const auto &vertex : hex) {
                    float angle = std::atan2(vertex.y - center.y, vertex.x - center.x);
                    angleVertices.push_back({angle, vertex});
                }
                // Sort so that the “top” vertex (largest angle) is first.
                std::sort(angleVertices.begin(), angleVertices.end(),
                          [](const std::pair<float, glm::vec3>& a, const std::pair<float, glm::vec3>& b) {
                              return a.first > b.first;
                          });

                // Assign alternating colors: red for even indices, green for odd.
                for (size_t localIdx = 0; localIdx < angleVertices.size(); localIdx++) {
                    const glm::vec3 &vertex = angleVertices[localIdx].second;
                    int baseVertexIndex = findApproxVertexIndex(baseUniquePositions, vertex);
                    if (baseVertexIndex != -1) {
                        size_t globalIdx = layer * baseCount + baseVertexIndex;
                        if (localIdx % 2 == 0) {
                            soA.color[globalIdx] = glm::vec3(1.0f, 0.0f, 0.0f); // red
                        } else {
                            soA.color[globalIdx] = glm::vec3(0.0f, 1.0f, 0.0f); // green
                        }
                    }
                }
            }
        }
    }
    else {
        // Fallback: simple alternating colors if no hexagon lists are available.
        for (size_t i = 0; i < totalParticles; i++) {
            soA.color[i] = (i % 2 == 0) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    // --- Set Static Particles Along the Left and (optionally) Right Edges ---
    // Determine a horizontal axis by projecting the global x-axis onto the plane perpendicular to verticalAxis.
    glm::vec3 globalX = glm::vec3(1,0,0);
    glm::vec3 horizontalAxis = globalX - (glm::dot(globalX, verticalAxis) * verticalAxis);
    const float epsilon = 0.0001f;
    if (glm::length(horizontalAxis) < epsilon) {
        // In case verticalAxis is nearly parallel to globalX, use globalY.
        horizontalAxis = glm::vec3(0,1,0) - (glm::dot(glm::vec3(0,1,0), verticalAxis) * verticalAxis);
    }
    horizontalAxis = glm::normalize(horizontalAxis);

    // For each layer, find the min and max projections along the horizontal axis.
    // Then, mark all particles whose projection is within a tolerance of these extremes.
    const float edgeTolerance = 0.5f;  // Adjust as needed.
    for (int layer = 0; layer < nLayers; layer++) {
        size_t layerStart = layer * baseCount;
        size_t layerEnd = layerStart + baseCount;
        float minProj = std::numeric_limits<float>::max();
        float maxProj = -std::numeric_limits<float>::max();
        // First pass: determine min and max projections.
        for (size_t i = layerStart; i < layerEnd; i++) {
            float proj = glm::dot(soA.position[i], horizontalAxis);
            if (proj < minProj) {
                minProj = proj;
            }
            if (proj > maxProj) {
                maxProj = proj;
            }
        }
        // Second pass: mark all particles within tolerance of the min and max as static.
        for (size_t i = layerStart; i < layerEnd; i++) {
            float proj = glm::dot(soA.position[i], horizontalAxis);
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
            float dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5f;
            springs.push_back(sp);
        }
    }

    // --- Inter-layer Springs ---
    if (!hexagonVertexLists.empty()) {
        for (int layer = 0; layer < nLayers - 1; layer++) {
            for (const auto &hex : hexagonVertexLists) {
                std::vector<size_t> lowerGreenIndices;
                std::vector<glm::vec3> lowerGreenPositions;
                std::vector<size_t> upperRedIndices;
                std::vector<glm::vec3> upperRedPositions;
                
                for (const glm::vec3 &baseVertex : hex) {
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, baseVertex);
                    if (baseIndex == -1)
                        continue;
                    size_t lowerGlobalIndex = layer * baseCount + baseIndex;
                    size_t upperGlobalIndex = (layer + 1) * baseCount + baseIndex;
                    
                    if (approxEqualVec3(soA.color[lowerGlobalIndex], glm::vec3(0.0f, 1.0f, 0.0f))) {
                        lowerGreenIndices.push_back(lowerGlobalIndex);
                        lowerGreenPositions.push_back(soA.position[lowerGlobalIndex]);
                    }
                    if (approxEqualVec3(soA.color[upperGlobalIndex], glm::vec3(1.0f, 0.0f, 0.0f))) {
                        upperRedIndices.push_back(upperGlobalIndex);
                        upperRedPositions.push_back(soA.position[upperGlobalIndex]);
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
                        float dist = glm::distance(soA.position[sp.p1Index], soA.position[sp.p2Index]);
                        sp.restLength = dist * springRestLength;
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
                        float dist = glm::distance(soA.position[sp.p1Index], soA.position[sp.p2Index]);
                        sp.restLength = dist * springRestLength;
                        sp.springConstant = springConstant;
                        sp.damping = 0.5f;
                        springs.push_back(sp);
                    }
                }
            }
        }
    }
}

glm::mat4 Simulation::createRotationMatrix(float orientationDegrees) {
    return glm::rotate(glm::mat4(1.0f),
                       glm::radians(orientationDegrees),
                       glm::vec3(1.f, 0.f, 0.f));
}

void Simulation::generateHexagonCells(int numHexagons,
                                      float hexagonSize,
                                      const glm::mat4& rotationMatrix,
                                      std::vector<glm::vec3>& uniquePositions,
                                      std::set<std::pair<int,int>>& edgeSet)
{
    // Local lambda to check if a vertex already exists
    auto findOrAdd = [&](const glm::vec3 &pos) -> int {
        const float eps = 0.0001f;
        for (int i = 0; i < (int)uniquePositions.size(); i++) {
            if (glm::length(uniquePositions[i] - pos) < eps)
                return i;
        }
        uniquePositions.push_back(pos);
        return static_cast<int>(uniquePositions.size()) - 1;
    };

    // Loop over rows and columns of hexagons
    for (int r = 0; r < numHexagons; r++) {
        for (int c = 0; c < numHexagons; c++) {
            // Compute the center of the current hexagon
            glm::vec3 center;
            center.x = sqrt(3.f) * hexagonSize * (c + (r % 2) * 0.5f);
            center.y = 1.5f * hexagonSize * r;
            center.z = 0.f;
            center = glm::vec3(rotationMatrix * glm::vec4(center, 1.f));

            // Compute the six vertices for this hexagon
            std::vector<glm::vec3> currentVerts;
            std::vector<int> indices;
            currentVerts.reserve(6);
            indices.reserve(6);
            for (int i = 0; i < 6; i++) {
                float ang = glm::radians(60.f * i + 90.f);
                glm::vec3 offset(hexagonSize * cos(ang),
                                 hexagonSize * sin(ang),
                                 0.f);
                offset = glm::vec3(rotationMatrix * glm::vec4(offset, 0.f));
                glm::vec3 vertex = center + offset;
                currentVerts.push_back(vertex);
                indices.push_back(findOrAdd(vertex));
            }
            // Save the vertices and their indices for rendering later.
            hexagonVertexLists.push_back(currentVerts);
            hexagonIndices.push_back(indices);

            // Record each edge (make sure the lower index comes first)
            for (int i = 0; i < 6; i++) {
                int idx1 = indices[i];
                int idx2 = indices[(i + 1) % 6];
                if (idx1 > idx2)
                    std::swap(idx1, idx2);
                edgeSet.insert({ idx1, idx2 });
            }
        }
    }
}

void Simulation::assignUniquePositionsToSoA(const std::vector<glm::vec3>& uniquePositions, bool bothEndsStatic) {
    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::vec3(0));
    soA.forceAccum.resize(n, glm::vec3(0));
    soA.mass.resize(n, 10.f);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::vec3(1.0f, 0.4f, 0.0f));
    soA.dimensions.resize(n, glm::vec3(1.0f));

    float minX = 1e9f, maxX = -1e9f;
    for (const auto &p : uniquePositions) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    for (size_t i = 0; i < n; i++) {
        soA.position[i] = uniquePositions[i];
        bool leftStatic = (soA.position[i].x <= minX + 0.001f);
        bool rightStatic = (bothEndsStatic && soA.position[i].x >= maxX - 0.001f);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }
}

void Simulation::createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, float springRestLength) {
    for (const auto &e : edgeSet) {
        int i1 = e.first;
        int i2 = e.second;
        float dist = glm::distance(soA.position[i1], soA.position[i2]);
        SpringData sp;
        sp.p1Index = i1;
        sp.p2Index = i2;
        sp.restLength = dist * springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5f;
        springs.push_back(sp);
    }
}

void Simulation::createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength) {
    clearSimulation();
    int rows = gridSize + 1;
    int cols = gridSize + 1;
    soA.position.reserve(rows * cols);
    soA.velocity.reserve(rows * cols);
    soA.forceAccum.reserve(rows * cols);
    soA.mass.reserve(rows * cols);
    soA.type.reserve(rows * cols);
    soA.isStatic.reserve(rows * cols);
    soA.color.reserve(rows * cols);
    soA.dimensions.reserve(rows * cols);
    
    auto getIndex = [&](int r, int c) {
        return r * cols + c;
    };
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            glm::vec3 pos(c * spacing, r * spacing, 0.f);
            soA.position.push_back(pos);
            soA.velocity.push_back(glm::vec3(0));
            soA.forceAccum.push_back(glm::vec3(0));
            soA.mass.push_back(10.f);
            soA.type.push_back(ParticleType::STRUCTURE);
            bool sflag = (c == 0);
            soA.isStatic.push_back(sflag);
            soA.color.push_back(glm::vec3(1,0,0));
            soA.dimensions.push_back(glm::vec3(1.f));
        }
    }
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols - 1; c++) {
            int i1 = getIndex(r, c);
            int i2 = getIndex(r, c + 1);
            float dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5f;
            springs.push_back(sp);
        }
    }
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols; c++) {
            int i1 = getIndex(r, c);
            int i2 = getIndex(r + 1, c);
            float dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5f;
            springs.push_back(sp);
        }
    }
    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            int idxTL = getIndex(r, c);
            int idxTR = getIndex(r, c + 1);
            int idxBL = getIndex(r + 1, c);
            int idxBR = getIndex(r + 1, c + 1);
            
            float distTLBR = glm::distance(soA.position[idxTL], soA.position[idxBR]);
            SpringData s1;
            s1.p1Index = idxTL;
            s1.p2Index = idxBR;
            s1.restLength = distTLBR * springRestLength;
            s1.springConstant = springConstant;
            s1.damping = 0.5f;
            springs.push_back(s1);
            
            float distTRBL = glm::distance(soA.position[idxTR], soA.position[idxBL]);
            SpringData s2;
            s2.p1Index = idxTR;
            s2.p2Index = idxBL;
            s2.restLength = distTRBL * springRestLength;
            s2.springConstant = springConstant;
            s2.damping = 0.5f;
            springs.push_back(s2);
        }
    }
}

void Simulation::updateHexTriangles()
{
    // Thread-safety for the SoA and internal vectors
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    // Clear old data
    hexTriangles.clear();

    // If no hex data, no work
    const size_t totalHexes = hexagonIndices.size();
    if (totalHexes == 0)
        return;

    // Decide how many worker threads
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) 
        numThreads = 2;

    // Plan the batch size per thread
    size_t chunkSize = (totalHexes + numThreads - 1) / numThreads;

    // We'll store partial results from each thread, then merge them
    std::vector<std::future<std::vector<HexTriangle>>> futures;
    futures.reserve(numThreads);

    // Enqueue parallel tasks
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end   = std::min(start + chunkSize, totalHexes);
        if (start >= end)
            break; // No more hexes to process for this thread

        // Capture everything we need by [this] and by value
        futures.push_back(
            threadPool->enqueue([this, start, end]() -> std::vector<HexTriangle>
            {
                std::vector<HexTriangle> localTris;
                localTris.reserve((end - start) * 6); 
                // Each hex -> 6 triangles in a fan around its center

                for (size_t h = start; h < end; h++) {
                    // Indices of the 6 corners for hex h
                    const std::vector<int>& inds = hexagonIndices[h];

                    // Collect the corner positions from the SoA
                    glm::vec3 corners[6];
                    for (int i = 0; i < 6; i++) {
                        corners[i] = soA.position[inds[i]];
                    }

                    // Compute the hex center
                    glm::vec3 center(0.0f);
                    for (int i = 0; i < 6; i++) {
                        center += corners[i];
                    }
                    center /= 6.0f;

                    // Build 6 triangles (a fan from the center)
                    for (int i = 0; i < 6; i++) {
                        HexTriangle tri;
                        tri.vertices[0] = center;
                        tri.vertices[1] = corners[i];
                        tri.vertices[2] = corners[(i + 1) % 6];

                        // Compute the normal via cross product
                        glm::vec3 edge1 = tri.vertices[1] - tri.vertices[0];
                        glm::vec3 edge2 = tri.vertices[2] - tri.vertices[0];
                        tri.normal = glm::normalize(glm::cross(edge1, edge2));

                        localTris.push_back(tri);
                    }
                }
                return localTris;
            })
        );
    }

    // Gather partial results from each thread
    size_t totalTriCount = 0;
    std::vector<std::vector<HexTriangle>> partials(numThreads);
    for (size_t i = 0; i < futures.size(); i++) {
        std::vector<HexTriangle> threadOutput = futures[i].get();
        totalTriCount += threadOutput.size();
        partials[i] = std::move(threadOutput);
    }

    // Reserve once, then append all partial vectors
    hexTriangles.reserve(totalTriCount);
    for (auto& part : partials) {
        hexTriangles.insert(hexTriangles.end(), part.begin(), part.end());
    }
}

void Simulation::startAsyncUpdates() {
    asyncRunning = true;
    asyncThread = std::thread(&Simulation::asyncLoop, this);
}

void Simulation::stopAsyncUpdates() {
    asyncRunning = false;
    if (asyncThread.joinable())
        asyncThread.join();
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
            update((float)dt);
        }
        stepsCount++;
        lastPhysicsUpdateTime.store((float)dt);
        if (now - lastMeasure >= 1.0) {
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

// Convenience getter for hexagon triangles.
const std::vector<HexTriangle>& Simulation::getHexTriangles() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return hexTriangles;
}

// Convenience getter for structure particle positions.
std::vector<glm::vec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> ret;
    for (size_t i = 0; i < soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE)
            ret.push_back(soA.position[i]);
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