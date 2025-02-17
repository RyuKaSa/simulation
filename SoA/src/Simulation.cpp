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

// ===== SIMPLE THREAD POOL IMPLEMENTATION ===== //
class ThreadPool {
public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this](){
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this]{ return stop || !tasks.empty(); });
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

// ===== Simulation Class Implementation ===== //

Simulation::Simulation() {
    // Create a thread pool once (reuse threads across updates)
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;
    threadPool = new ThreadPool(numThreads);
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
    createHexGrid(60, 0.3f, 1.0f, false, 10.0f);
    // Gravity link
    if (gravityLink) { delete gravityLink; }
    gravityLink = new Link(soA, glm::vec3(40.0f, 0.0f, 0.0f));
}

void Simulation::reset() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "RESET initialized" << std::endl;

    update(0.0f);
    clearSimulation(); 
    // Reinitialize
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
    balls.clear();
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

void Simulation::update(float dt) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;
    
    // --- 1) Parallel Spring Update using thread pool ---
    size_t totalSprings = springs.size();
    size_t chunkSize = (totalSprings + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;

    // Zero the force accumulation.
    for (auto &f : soA.forceAccum) {
        f = glm::vec3(0.0f);
    }

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
                    if (dist < 1e-7f) continue;

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
                    // Note: non-atomic update; assuming no data races on distinct indices.
                    soA.forceAccum[i1] += totalForce;
                    soA.forceAccum[i2] -= totalForce;
                }
            })
        );
    }
    for (auto &f : futures) { f.get(); }
    futures.clear();

    // --- 2) Gravity Link Update (serial) ---
    if (gravityLink) {
        applyGravityLink();
    }

    // --- 3) Parallel Particle Integration using thread pool and SIMD hint ---
    size_t n = soA.position.size();
    size_t chunkPart = (n + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkPart;
        size_t end   = std::min(start + chunkPart, n);
        futures.push_back(
            threadPool->enqueue([this, dt, start, end]() {
                // Hint to the compiler to auto-vectorize the following loop.
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
    for (auto &f : futures) { f.get(); }
    futures.clear();

    // --- 4) Update legacy Ball vector for rendering queries (serial) ---
    balls.clear();
    balls.resize(n);
    for (size_t i = 0; i < n; i++) {
        Ball b;
        b.position = soA.position[i];
        b.color    = (i < soA.color.size()) ? soA.color[i] : glm::vec3(1, 0, 0);
        b.type     = soA.type[i];
        b.dimensions = (i < soA.dimensions.size()) ? soA.dimensions[i] : glm::vec3(1.f);
        balls[i]   = b;
    }
    
    // --- 5) (Optionally) Update hexagon triangles from the current particle positions ---
    // updateHexTriangles();
}

void Simulation::applyGravityLink() {
    if (gravityLink) {
        gravityLink->applyGravity();
    }
}

void Simulation::createCord(int numBalls, float length,
                            float springRestLength, bool bothEndsStatic_) {
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

void Simulation::createHexGrid(int numHexagons, float hexagonSize, float springRestLength,
                               bool bothEndsStatic_, float orientationDegrees) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;
    std::vector<glm::vec3> uniquePositions;
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    auto rotationMatrix = glm::rotate(glm::mat4(1.0f),
                                      glm::radians(orientationDegrees),
                                      glm::vec3(1.f, 0.f, 0.f));

    auto findOrAdd = [&](const glm::vec3 &pos) -> int {
        const float eps = 0.0001f;
        for (int i = 0; i < (int)uniquePositions.size(); i++) {
            if (glm::length(uniquePositions[i] - pos) < eps) {
                return i;
            }
        }
        uniquePositions.push_back(pos);
        return (int)uniquePositions.size() - 1;
    };

    std::set<std::pair<int,int>> edgeSet;
    // For each hexagon, also record its vertices and indices.
    for (int r = 0; r < numHexagons; r++) {
        for (int c = 0; c < numHexagons; c++) {
            glm::vec3 center;
            center.x = sqrt(3.f) * hexagonSize * (c + (r % 2) * 0.5f);
            center.y = 1.5f * hexagonSize * r;
            center.z = 0.f;
            center = glm::vec3(rotationMatrix * glm::vec4(center, 1.f));

            std::vector<glm::vec3> currentVerts;
            std::vector<int> indices;
            currentVerts.reserve(6);
            indices.reserve(6);
            for (int i = 0; i < 6; i++) {
                float ang = glm::radians(60.f * i + 90.f);
                glm::vec3 off(hexagonSize * cos(ang),
                              hexagonSize * sin(ang),
                              0.f);
                off = glm::vec3(rotationMatrix * glm::vec4(off, 0.f));
                glm::vec3 vertex = center + off;
                currentVerts.push_back(vertex);
                indices.push_back(findOrAdd(vertex));
            }
            hexagonVertexLists.push_back(currentVerts);
            hexagonIndices.push_back(indices);

            // Also record edges for springs
            for (int i = 0; i < 6; i++) {
                int idx1 = indices[i];
                int idx2 = indices[(i+1)%6];
                if (idx1 > idx2) std::swap(idx1, idx2);
                edgeSet.insert({idx1, idx2});
            }
        }
    }

    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::vec3(0));
    soA.forceAccum.resize(n, glm::vec3(0));
    soA.mass.resize(n, 10.f);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::vec3(1, 0, 0));
    soA.dimensions.resize(n, glm::vec3(3.f));

    float minX = 1e9f, maxX = -1e9f;
    for (auto &p : uniquePositions) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    for (size_t i = 0; i < n; i++) {
        soA.position[i] = uniquePositions[i];
        bool leftStatic = (soA.position[i].x <= minX + 0.001f);
        bool rightStatic = (bothEndsStatic_ && soA.position[i].x >= maxX - 0.001f);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }

    for (auto &e : edgeSet) {
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
    
    // Initially compute hexagon triangles.
    // updateHexTriangles();
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

const std::vector<glm::vec3> Simulation::getParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return std::vector<glm::vec3>(soA.velocity.begin(), soA.velocity.end());
}

const std::vector<glm::vec3> Simulation::getStructureParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> ret;
    for (size_t i = 0; i < soA.velocity.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            ret.push_back(soA.velocity[i]);
        }
    }
    return ret;
}

std::vector<glm::vec3> Simulation::getParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> pos(soA.position.begin(), soA.position.end());
    return pos;
}

std::vector<glm::vec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> ret;
    for (size_t i = 0; i < soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            ret.push_back(soA.position[i]);
        }
    }
    return ret;
}

std::vector<glm::vec3> Simulation::getSpringEndpoints() const {
    std::vector<glm::vec3> endpoints;
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    endpoints.reserve(springs.size() * 2);
    for (auto &sp : springs) {
        endpoints.push_back(soA.position[sp.p1Index]);
        endpoints.push_back(soA.position[sp.p2Index]);
    }
    return endpoints;
}

const std::vector<Ball>& Simulation::getBalls() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return balls;
}

const std::vector<Ball>& Simulation::getSnapshotBalls() const {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return snapshotBalls;
}

BallSoA Simulation::getSnapshotSoA() const {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return snapshotSoA;
}

BallSoA Simulation::convertBallsToSoA(const std::vector<Ball>& inBalls) const {
    BallSoA soa;
    size_t n = inBalls.size();
    soa.resize(n);
    for (size_t i = 0; i < n; i++) {
        soa.posX[i]   = inBalls[i].position.x;
        soa.posY[i]   = inBalls[i].position.y;
        soa.posZ[i]   = inBalls[i].position.z;
        soa.colorR[i] = inBalls[i].color.r;
        soa.colorG[i] = inBalls[i].color.g;
        soa.colorB[i] = inBalls[i].color.b;
        soa.types[i]  = (int)inBalls[i].type;
        soa.dimsX[i]  = inBalls[i].dimensions.x;
        soa.dimsY[i]  = inBalls[i].dimensions.y;
        soa.dimsZ[i]  = inBalls[i].dimensions.z;
    }
    return soa;
}

const std::vector<HexTriangle>& Simulation::getHexTriangles() const {
    return hexTriangles;
}

void Simulation::updateHexTriangles() {
    hexTriangles.clear();
    // For each hexagon (as defined by hexagonIndices), compute triangles
    for (size_t h = 0; h < hexagonIndices.size(); h++) {
        const std::vector<int>& indices = hexagonIndices[h];
        if (indices.size() < 6) continue;
        std::vector<glm::vec3> currentVerts;
        currentVerts.reserve(indices.size());
        for (int idx : indices) {
            currentVerts.push_back(soA.position[idx]);
        }
        // Compute center of hexagon
        glm::vec3 center(0.0f);
        for (const auto &v : currentVerts) {
            center += v;
        }
        center /= (float)currentVerts.size();
        // Create 6 triangles (center, vertex[i], vertex[(i+1)%6])
        for (int i = 0; i < 6; i++) {
            HexTriangle tri;
            tri.vertices[0] = center;
            tri.vertices[1] = currentVerts[i];
            tri.vertices[2] = currentVerts[(i+1)%6];
            glm::vec3 edge1 = tri.vertices[1] - center;
            glm::vec3 edge2 = tri.vertices[2] - center;
            tri.normal = glm::normalize(glm::cross(edge1, edge2));
            hexTriangles.push_back(tri);
        }
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
            update((float)dt);
        }
        stepsCount++;

        {
            std::lock_guard<std::mutex> snapLock(snapshotMutex);
            snapshotBalls = balls;
            snapshotSoA = convertBallsToSoA(balls);
        }
        lastPhysicsUpdateTime.store((float)dt);

        if (now - lastMeasure >= 1.0) {
            effectiveStepsPerSecond.store(stepsCount);
            stepsCount = 0;
            lastMeasure = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}