#include "Simulation.hpp"
#include "SimulationSoAInternals.hpp"
#include <thread>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <omp.h>

extern double getCurrentTime();

Simulation::Simulation() {
    Initialization();
}

Simulation::~Simulation() {
    clearSimulation();
    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

void Simulation::Initialization() {
    // Example: create a default hex grid
    createHexGrid(50, 1.0f, 0.9f, false, 10.0f);
    // Gravity link
    if (gravityLink) { delete gravityLink; }
    gravityLink = new Link(soA, glm::vec3(40.0f, 0.0f, 0.0f)); 
}

void Simulation::reset() {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::cout << "RESET initialized" << std::endl;

    update(0.0f); // Force an update cycle
    clearSimulation(); 
    // Reinitialize
    Initialization();
}

void Simulation::clearSimulation() {
    std::cout << "Clearing simulation..." << std::endl;
    // Remove all SoA data
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
    hexFaces.clear();
    hexagonIndices.clear();
    balls.clear();

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

void Simulation::setSpringConstant(float k) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    springConstant = k;
    // Update all SoA-based springs
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

    // 1) Parallel spring update
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;
    size_t totalSprings = springs.size();
    size_t chunkSize = (totalSprings + numThreads - 1) / numThreads;

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    // Pre-zero the forceAccum in case the last frame left anything
    for (auto &f : soA.forceAccum) {
        f = glm::vec3(0.0f);
    }

    // Lambda for parallel spring chunk
    auto springWorker = [this](size_t start, size_t end) {
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

            // Equivalent to your Conditional_Update logic
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
            // Accumulate
            soA.forceAccum[i1] += totalForce;
            soA.forceAccum[i2] -= totalForce;
        }
    };

    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end   = std::min(start + chunkSize, totalSprings);
        threads.emplace_back(springWorker, start, end);
    }
    for (auto &th : threads) { th.join(); }
    threads.clear();

    // 2) Gravity link
    if (gravityLink) {
        applyGravityLink();
    }

    // 3) collision update
    updateHexFaces();
    collisionDetectionAndResolution(dt);

    // 4) Parallel particle integration
    size_t n = soA.position.size();
    size_t chunkPart = (n + numThreads - 1) / numThreads;

    auto particleWorker = [this, dt](size_t start, size_t end) {
        for (size_t i = start; i < end; i++) {
            if (soA.isStatic[i]) {
                // fixed
                soA.forceAccum[i] = glm::vec3(0);
            } else {
                float m = soA.mass[i];
                glm::vec3 accel = soA.forceAccum[i] / m;
                soA.velocity[i] += accel * dt;
                soA.position[i] += soA.velocity[i] * dt;
                soA.forceAccum[i] = glm::vec3(0.0f);
            }
        }
    };

    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkPart;
        size_t end   = std::min(start + chunkPart, n);
        threads.emplace_back(particleWorker, start, end);
    }
    for (auto &th : threads) { th.join(); }

    // 5) Also update the `balls` array so that getBalls() works:
    balls.clear();
    balls.resize(n);
    for (size_t i = 0; i < n; i++) {
        Ball b;
        b.position = soA.position[i];
        b.color    = (i < soA.color.size()) ? soA.color[i] : glm::vec3(1,0,0);
        b.type     = soA.type[i];
        b.dimensions = (i < soA.dimensions.size()) ? soA.dimensions[i] : glm::vec3(1.f);
        balls[i]   = b;
    }
}

void Simulation::updateHexFaces() {
    hexFaces.clear();
    // Recompute normals for each hex from soA.position
    // We have hexagonIndices: each element is 6 indices => for that hex
    for (size_t h = 0; h < hexagonIndices.size(); ++h) {
        const auto &hexIndices = hexagonIndices[h];
        std::vector<glm::vec3> currentHexVertices;
        currentHexVertices.reserve(hexIndices.size());
        for (int idx : hexIndices) {
            currentHexVertices.push_back(soA.position[idx]);
        }
        if (currentHexVertices.size() < 6) continue;

        // Center
        glm::vec3 center(0.f);
        for (auto &v : currentHexVertices) {
            center += v;
        }
        center /= (float)currentHexVertices.size();

        // 6 triangular faces
        for (int i = 0; i < 6; i++) {
            int next = (i + 1) % 6;
            HexFace face;
            face.triangle = { center, currentHexVertices[i], currentHexVertices[next] };
            glm::vec3 edge1 = currentHexVertices[i] - center;
            glm::vec3 edge2 = currentHexVertices[next] - center;
            face.normal = glm::normalize(glm::cross(edge1, edge2));
            face.hexagonIndex = (int)h;
            hexFaces.push_back(face);
        }
    }
}

void Simulation::collisionDetectionAndResolution(float dt)
{
    // Let’s say we no longer have a single constant offset:
    // We'll compute an offset = sphere radius for each ball. 
    // We'll also have frictionFactor and we can keep restitution as well.

    const float frictionFactor = 0.9f;
    const float restitution    = 0.5f; 

    std::vector<int> externalIndices;
    externalIndices.reserve(soA.position.size());
    for (int i = 0; i < (int)soA.position.size(); i++)
    {
        if (soA.type[i] == ParticleType::EXTERNAL) {
            externalIndices.push_back(i);
        }
    }

#pragma omp parallel for
    for (size_t idx = 0; idx < externalIndices.size(); idx++)
    {
        int iExt      = externalIndices[idx];
        glm::vec3 pos = soA.position[iExt];
        glm::vec3 vel = soA.velocity[iExt];

        // Example: assume soA.dimensions[iExt].x is the diameter or radius
        float radius = soA.dimensions[iExt].x * 0.5f;

        for (const HexFace& face : hexFaces)
        {
            glm::vec3 planePoint = face.triangle[0];
            glm::vec3 normal     = face.normal;
            float dist = glm::dot((pos - planePoint), normal);

            // For two-sided collisions:
            // float absDist = std::fabs(dist);
            // if (absDist > radius) continue;
            // glm::vec3 collisionNormal = (dist >= 0.0f) ? normal : -normal;

            // For one-sided collisions, check if we are within [0, radius].
            if (dist < 0.f || dist > radius) {
                continue; 
            }

            // Project the sphere center onto the plane:
            glm::vec3 projectedCenter = pos - dist * normal;
            // Check if projected center is inside the face polygon
            if (!isPointInTriangle(projectedCenter, face.triangle, normal)) {
                continue;
            }

            // If we reach here, we have a collision. Let's push out by the
            // overlap amount (which is radius - dist).
            float penetrationDepth = radius - dist;
            glm::vec3 correction   = normal * (penetrationDepth * impulseScaling);

            // Now the mass ratio splitting for structure vs external (like you had):
            float m_ext = soA.mass[iExt];
            const std::vector<int>& hexIdxs = hexagonIndices[face.hexagonIndex];
            float m_struct = 0.f;
            for (int hidx : hexIdxs) {
                m_struct += soA.mass[hidx];
            }
            float totalM = m_ext + m_struct;
            if (totalM < 1e-6f) totalM = 1e-6f;

            float extFrac    = m_struct / totalM; 
            float structFrac = m_ext   / totalM;

            // Move external
            soA.position[iExt] += (correction * extFrac);

            // Move structure
            glm::vec3 structCorr = -(correction * structFrac) / (float)hexIdxs.size();
            for (int hidx : hexIdxs) {
                soA.position[hidx] += structCorr;
            }

            // Now reflect velocities with friction or damping:
            // external velocity
            float vDotE = glm::dot(vel, normal);
            if (vDotE < 0.f)
            {
                // normal reflection
                vel = vel - (1.f + restitution)*vDotE*normal;
                // friction: reduce tangential
                glm::vec3 tangentialE = vel - glm::dot(vel, normal)*normal;
                vel -= frictionFactor * tangentialE;
            }
            soA.velocity[iExt] = vel;

            // structure velocity (for each hidx)
            for (int hidx : hexIdxs) {
                glm::vec3 &vStruct = soA.velocity[hidx];
                float vDotS = glm::dot(vStruct, normal);
                if (vDotS < 0.f) {
                    // normal reflection
                    vStruct = vStruct - (1.f + restitution)*vDotS*normal;
                    // friction
                    glm::vec3 tangentialS = vStruct - glm::dot(vStruct,normal)*normal;
                    vStruct -= frictionFactor * tangentialS;
                }
            }
        } // end for each face
    } // end for each external
}

bool Simulation::isPointInTriangle(const glm::vec3& point,
                                   const std::array<glm::vec3, 3>& tri,
                                   const glm::vec3& /*normal*/) const {
    const float epsilon = 1e-6f;
    glm::vec3 edge0 = tri[1] - tri[0];
    glm::vec3 edge1 = tri[2] - tri[0];
    glm::vec3 vec   = point - tri[0];

    float d00 = glm::dot(edge0, edge0);
    float d01 = glm::dot(edge0, edge1);
    float d11 = glm::dot(edge1, edge1);
    float d20 = glm::dot(vec, edge0);
    float d21 = glm::dot(vec, edge1);
    float denom = d00*d11 - d01*d01;
    if (fabs(denom) < epsilon) return false;

    float u = (d11*d20 - d01*d21) / denom;
    float v = (d00*d21 - d01*d20) / denom;
    return (u >= 0.f) && (v >= 0.f) && (u+v <= 1.f);
}

void Simulation::applyGravityLink() {
    // Link has a pointer to soA, so it modifies soA.forceAccum
    if (gravityLink) {
        gravityLink->applyGravity();
    }
}

void Simulation::createCord(int numBalls, float length,
                            float springRestLength, bool bothEndsStatic_) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic_;
    // Reserve
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

    // Create particles
    for (int i = 0; i < numBalls; i++) {
        glm::vec3 pos = startPos + glm::vec3(i*spacing, 0.f, 0.f);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::vec3(0));
        soA.forceAccum.push_back(glm::vec3(0));
        soA.mass.push_back(10.f);
        soA.type.push_back(ParticleType::STRUCTURE);
        bool sflag = (bothEndsStatic_) ? (i==0 || i==(numBalls-1)) : (i==0);
        soA.isStatic.push_back(sflag);
        soA.color.push_back(glm::vec3(1,0,0));
        soA.dimensions.push_back(glm::vec3(3.f));
    }

    // Create springs
    for (int i = 0; i < numBalls-1; i++) {
        float dist = glm::distance(soA.position[i], soA.position[i+1]);
        SpringData sp;
        sp.p1Index = i;
        sp.p2Index = i+1;
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
    hexagonVertexLists.clear();
    hexFaces.clear();
    hexagonIndices.clear();
    springs.clear();

    auto rotationMatrix = glm::rotate(glm::mat4(1.0f),
                                      glm::radians(orientationDegrees),
                                      glm::vec3(1.f,0.f,0.f));

    auto findOrAdd = [&](const glm::vec3 &pos)->int {
        const float eps = 0.0001f;
        for (int i=0; i<(int)uniquePositions.size(); i++) {
            if (glm::length(uniquePositions[i] - pos) < eps) {
                return i;
            }
        }
        uniquePositions.push_back(pos);
        return (int)uniquePositions.size()-1;
    };

    // generate hex
    std::set<std::pair<int,int>> edgeSet;
    for (int r = 0; r < numHexagons; r++) {
        for (int c = 0; c < numHexagons; c++) {
            glm::vec3 center;
            center.x = sqrt(3.f)*hexagonSize*(c + (r%2)*0.5f);
            center.y = 1.5f*hexagonSize*r;
            center.z = 0.f;
            center = glm::vec3(rotationMatrix*glm::vec4(center,1.f));

            std::vector<glm::vec3> currentVerts;
            std::vector<int> indices;
            currentVerts.reserve(6);
            indices.reserve(6);
            for (int i = 0; i < 6; i++) {
                float ang = glm::radians(60.f*i + 90.f);
                glm::vec3 off(hexagonSize*std::cos(ang),
                              hexagonSize*std::sin(ang),
                              0.f);
                off = glm::vec3(rotationMatrix*glm::vec4(off,0.f));
                glm::vec3 vertex = center + off;
                currentVerts.push_back(vertex);
                indices.push_back(findOrAdd(vertex));
            }
            hexagonVertexLists.push_back(currentVerts);
            hexagonIndices.push_back(indices);

            // edges
            for (int i=0; i<6; i++) {
                int idx1 = indices[i];
                int idx2 = indices[(i+1)%6];
                if (idx1>idx2) std::swap(idx1, idx2);
                edgeSet.insert({idx1, idx2});
            }
        }
    }

    // Create SoA particles from uniquePositions
    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::vec3(0));
    soA.forceAccum.resize(n, glm::vec3(0));
    soA.mass.resize(n, 10.f);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::vec3(1,0,0));
    soA.dimensions.resize(n, glm::vec3(3.f));

    // find minX, maxX
    float minX = 1e9f, maxX = -1e9f;
    for (auto &p : uniquePositions) {
        if (p.x<minX) minX = p.x;
        if (p.x>maxX) maxX = p.x;
    }
    for (size_t i=0; i<n; i++) {
        soA.position[i] = uniquePositions[i];
        // static flags
        bool leftStatic = (soA.position[i].x <= minX+0.001f);
        bool rightStatic = (bothEndsStatic_ && soA.position[i].x >= maxX-0.001f);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }

    // Create springs from edgeSet
    for (auto &e : edgeSet) {
        int i1 = e.first;
        int i2 = e.second;
        float dist = glm::distance(soA.position[i1], soA.position[i2]);
        SpringData sp;
        sp.p1Index = i1;
        sp.p2Index = i2;
        sp.restLength = dist*springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5f;
        springs.push_back(sp);
    }

    // Build the hex collision faces once
    for (auto &hv : hexagonVertexLists) {
        if (hv.size() < 6) continue;
        glm::vec3 c(0.f);
        for (auto &v : hv) c += v;
        c /= (float)hv.size();
        for (int i=0; i<6; i++) {
            int nx = (i+1)%6;
            HexFace f;
            f.triangle = {c, hv[i], hv[nx]};
            glm::vec3 e1 = hv[i] - c;
            glm::vec3 e2 = hv[nx] - c;
            f.normal = glm::normalize(glm::cross(e1,e2));
            f.hexagonIndex = 0; // will update in updateHexFaces
            hexFaces.push_back(f);
        }
    }
}

void Simulation::createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength) {
    clearSimulation();
    int rows = gridSize+1;
    int cols = gridSize+1;
    soA.position.reserve(rows*cols);
    soA.velocity.reserve(rows*cols);
    soA.forceAccum.reserve(rows*cols);
    soA.mass.reserve(rows*cols);
    soA.type.reserve(rows*cols);
    soA.isStatic.reserve(rows*cols);
    soA.color.reserve(rows*cols);
    soA.dimensions.reserve(rows*cols);

    auto getIndex = [&](int r, int c) {
        return r*cols + c;
    };

    // Create grid
    for (int r=0; r<rows; r++) {
        for (int c=0; c<cols; c++) {
            glm::vec3 pos(c*spacing, r*spacing, 0.f);
            soA.position.push_back(pos);
            soA.velocity.push_back(glm::vec3(0));
            soA.forceAccum.push_back(glm::vec3(0));
            soA.mass.push_back(10.f);
            soA.type.push_back(ParticleType::STRUCTURE);
            bool sflag = (c==0); 
            soA.isStatic.push_back(sflag);
            soA.color.push_back(glm::vec3(1,0,0));
            soA.dimensions.push_back(glm::vec3(1.f));
        }
    }

    // Springs
    for (int r=0; r<rows; r++) {
        for (int c=0; c<cols-1; c++) {
            int i1 = getIndex(r,c);
            int i2 = getIndex(r,c+1);
            float dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist*springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5f;
            springs.push_back(sp);
        }
    }
    for (int r=0; r<rows-1; r++) {
        for (int c=0; c<cols; c++) {
            int i1 = getIndex(r,c);
            int i2 = getIndex(r+1,c);
            float dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist*springRestLength;
            sp.springConstant = springConstant;
            sp.damping = 0.5f;
            springs.push_back(sp);
        }
    }
    // diagonals
    for (int r=0; r<rows-1; r++) {
        for (int c=0; c<cols-1; c++) {
            int idxTL = getIndex(r,c);
            int idxTR = getIndex(r,c+1);
            int idxBL = getIndex(r+1,c);
            int idxBR = getIndex(r+1,c+1);

            float distTLBR = glm::distance(soA.position[idxTL], soA.position[idxBR]);
            SpringData s1;
            s1.p1Index = idxTL;
            s1.p2Index = idxBR;
            s1.restLength = distTLBR*springRestLength;
            s1.springConstant = springConstant;
            s1.damping = 0.5f;
            springs.push_back(s1);

            float distTRBL = glm::distance(soA.position[idxTR], soA.position[idxBL]);
            SpringData s2;
            s2.p1Index = idxTR;
            s2.p2Index = idxBL;
            s2.restLength = distTRBL*springRestLength;
            s2.springConstant = springConstant;
            s2.damping = 0.5f;
            springs.push_back(s2);
        }
    }
}

void Simulation::throwBall(const glm::vec3& cameraPos, const glm::vec3& cameraDir,
                           float speed, float massVal, const glm::vec3& dims) {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    glm::vec3 spawnPos = cameraPos + cameraDir*1.f;
    glm::vec3 initialVel = cameraDir*speed;

    int i = (int)soA.position.size();
    soA.position.push_back(spawnPos);
    soA.velocity.push_back(initialVel);
    soA.forceAccum.push_back(glm::vec3(0));
    soA.mass.push_back(massVal);
    soA.type.push_back(ParticleType::EXTERNAL);
    soA.isStatic.push_back(false);
    soA.color.push_back(glm::vec3(0,1,0));
    soA.dimensions.push_back(dims);
}

std::vector<glm::vec3> Simulation::getParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> pos(soA.position.begin(), soA.position.end());
    return pos;
}
std::vector<glm::vec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> ret;
    for (size_t i=0; i<soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            ret.push_back(soA.position[i]);
        }
    }
    return ret;
}
const std::vector<glm::vec3> Simulation::getParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return std::vector<glm::vec3>(soA.velocity.begin(), soA.velocity.end());
}
const std::vector<glm::vec3> Simulation::getStructureParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> ret;
    for (size_t i=0; i<soA.velocity.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            ret.push_back(soA.velocity[i]);
        }
    }
    return ret;
}

std::vector<glm::vec3> Simulation::getSpringEndpoints() const {
    // Return each spring as two endpoints
    std::vector<glm::vec3> endpoints;
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    endpoints.reserve(springs.size()*2);
    for (auto &sp : springs) {
        endpoints.push_back(soA.position[sp.p1Index]);
        endpoints.push_back(soA.position[sp.p2Index]);
    }
    return endpoints;
}

std::vector<glm::vec3> Simulation::getHexHitboxTriangles() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> verts;
    // each HexFace is 3 vertices
    for (auto &hf : hexFaces) {
        for (int i=0; i<3; i++) {
            verts.push_back(hf.triangle[i]);
        }
    }
    return verts;
}

// For concurrency
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

        // Update snapshot
        {
            std::lock_guard<std::mutex> snapLock(snapshotMutex);
            // Convert the SoA-based balls vector to a SoA for rendering
            // but first we update `snapshotBalls = balls;`
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

// Return references to the local "balls" array for backward compatibility
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
    return snapshotSoA; // copy
}

// Re-use the old function that converts an AoS vector<Ball> into a BallSoA
BallSoA Simulation::convertBallsToSoA(const std::vector<Ball>& inBalls) const {
    BallSoA soa;
    size_t n = inBalls.size();
    soa.resize(n);
    for (size_t i=0; i<n; i++) {
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