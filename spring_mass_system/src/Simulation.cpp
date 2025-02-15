#include "Simulation.hpp"

Simulation::Simulation() : springConstant(0.5f) {
    Simulation::Initialization();
}

void Simulation::Initialization() {
    // createCord(50, 25.0f, 0.5f, true); // Default cord setup (3 balls, 1m length, both ends static)
    createHexGrid(100, 1.0f, 0.9f, false, 10.0f); // Default hex grid setup (20 hexagons, 1m size, both ends static)
    // createSquareGridWithDiagonals(50, 0.5f, 1.0f); // Default square grid setup (10x10 grid
    // gravityLink = new Link(ballObjects, glm::vec3(0.0f, -9.81f, 0.0f));
    gravityLink = new Link(ballObjects, glm::vec3(30.81f, 0.0f, 0.0f));
}

void Simulation::reset() {
    // Lock the simulation so that no update is running concurrently.
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    std::cout << "RESET initialized" << std::endl;
    // Force an update cycle to ensure all threads have completed.
    update(0.0f);
    clearSimulation(); // Delete springs, particles, and gravityLink.
    ballStaticFlags.clear();
    balls.clear();
    // Reinitialize the simulation.
    Initialization();
}

Simulation::~Simulation() {
    clearSimulation();
    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

void Simulation::setSpringConstant(float k) {
    springConstant = k;
    for (Spring* spring : springs) {
        spring->setSpringConstant(k);
    }
}

void Simulation::setDampingCoefficient(float z) {
    for (Spring* spring : springs) {
        spring->setDampingCoefficient(z);
    }
}

void Simulation::update(float dt) {

    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    // --- Parallel Spring Update ---
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 2;
    std::vector<std::thread> threads;
    size_t numSprings = springs.size();
    size_t chunkSize = (numSprings + numThreads - 1) / numThreads;
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSize;
        size_t end = std::min(start + chunkSize, numSprings);
        threads.push_back(std::thread([this, start, end]() {
            for (size_t i = start; i < end; i++) {
                springs[i]->Conditional_Update();
            }
        }));
    }
    for (auto &th : threads) {
        th.join();
    }
    
    if (gravityLink)
        applyGravityLink();
    

    // --- collision Update ---
    updateHexFaces();
    collisionDetectionAndResolution(dt);
    
    // --- Parallel Particle Update ---
    size_t numParticles = ballObjects.size();
    size_t chunkSizeParticles = (numParticles + numThreads - 1) / numThreads;
    threads.clear();
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSizeParticles;
        size_t end = std::min(start + chunkSizeParticles, numParticles);
        threads.push_back(std::thread([this, dt, start, end]() {
            for (size_t i = start; i < end; i++) {
                bool isStatic;
                // Use stored flag if available; otherwise fallback.
                if (ballStaticFlags.size() == ballObjects.size()) {
                    isStatic = ballStaticFlags[i];
                } else {
                    isStatic = (bothEndsStatic ? (i == 0 || i == ballObjects.size() - 1) : (i == 0));
                }
                // Force external particles to be dynamic.
                if (ballObjects[i]->type == ParticleType::EXTERNAL)
                    isStatic = false;
                
                if (isStatic)
                    ballObjects[i]->update_fixed(dt);
                else
                    ballObjects[i]->update(dt);
            }
        }));
    }
    for (auto &th : threads) {
        th.join();
    }
    
    // Update the positions in the "balls" array
    for (size_t i = 0; i < ballObjects.size(); i++) {
        balls[i].position = ballObjects[i]->getPosition();
    }
}

void Simulation::updateHexFaces() {
    hexFaces.clear();
    // Use hexagonIndices to update hitboxes based on current particle positions.
    for (size_t h = 0; h < hexagonIndices.size(); ++h) {
        const auto& hexIndices = hexagonIndices[h];
        std::vector<glm::vec3> currentHexVertices;
        for (int idx : hexIndices) {
            currentHexVertices.push_back(ballObjects[idx]->getPosition());
        }
        // Compute the center of the hexagon.
        glm::vec3 center(0.0f);
        for (const auto& v : currentHexVertices) {
            center += v;
        }
        center /= static_cast<float>(currentHexVertices.size());
        
        // Recreate the 6 triangular faces (fan triangulation).
        for (int i = 0; i < 6; ++i) {
            int next = (i + 1) % 6;
            HexFace face;
            face.triangle = { center, currentHexVertices[i], currentHexVertices[next] };
            glm::vec3 edge1 = currentHexVertices[i] - center;
            glm::vec3 edge2 = currentHexVertices[next] - center;
            face.normal = glm::normalize(glm::cross(edge1, edge2));
            face.hexagonIndex = static_cast<int>(h);
            hexFaces.push_back(face);
        }
    }
}

void Simulation::createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic;
    float spacing = length / (numBalls - 1);
    glm::vec3 startPos(-length / 2, 0.0f, 0.0f);

    for (int i = 0; i < numBalls; i++) {
        // Determine if this ball should be static.
        bool isStatic = bothEndsStatic ? (i == 0 || i == numBalls - 1) : (i == 0);
        PMat* ball = new PMat(10.0f, startPos + glm::vec3(i * spacing, 0.0f, 0.0f));
        ballObjects.push_back(ball);
        Ball b;
        b.position = ball->getPosition();
        b.color = glm::vec3(1.0f, 0.0f, 0.0f);
        b.type = ball->type;
        balls.push_back(b);
        // Save the static flag for this ball.
        ballStaticFlags.push_back(isStatic);
    }

    for (int i = 0; i < numBalls - 1; i++) {
        Spring* spring = new Spring(ballObjects[i], ballObjects[i + 1], springConstant, springRestLength);
        spring->setDampingCoefficient(0.5f);
        springs.push_back(spring);
    }
}

void Simulation::createHexGrid(int numHexagons, float hexagonSize, float springRestLength, bool bothEndsStatic, float orientationDegrees) {
    clearSimulation();
    ballStaticFlags.clear();
    hexagonVertexLists.clear();
    hexFaces.clear();

    std::vector<glm::vec3> uniquePositions;
    auto findOrAddVertex = [&](const glm::vec3 &pos) -> int {
        const float eps = 0.001f;
        for (size_t i = 0; i < uniquePositions.size(); i++) {
            if (glm::length(uniquePositions[i] - pos) < eps)
                return static_cast<int>(i);
        }
        uniquePositions.push_back(pos);
        return uniquePositions.size() - 1;
    };

    std::set<std::pair<int,int>> edgeSet;

    // Create a rotation matrix that rotates by the given angle (in degrees)
    // around the X-axis. This tilts the grid out of the original XY plane.
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(orientationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));

    // Generate hexagon vertices and store them
    for (int r = 0; r < numHexagons; ++r) {
        for (int c = 0; c < numHexagons; ++c) {
            // Compute the hexagon's center in grid coordinates (XY plane)
            glm::vec3 center;
            center.x = sqrt(3.0f) * hexagonSize * (c + (r % 2) * 0.5f);
            center.y = 1.5f * hexagonSize * r;
            center.z = 0.0f;

            // Rotate the center around the X-axis to tilt the grid
            center = glm::vec3(rotationMatrix * glm::vec4(center, 1.0f));

            std::vector<glm::vec3> currentHexVertices;
            std::vector<int> hexVertexIndices;
            
            // Generate the 6 vertices for this hexagon
            for (int i = 0; i < 6; ++i) {
                float angle = glm::radians(60.0f * i + 90.0f);
                // Compute the vertex offset in local (unrotated) space
                glm::vec3 offset = glm::vec3(
                    hexagonSize * cos(angle),
                    hexagonSize * sin(angle),
                    0.0f
                );
                // Rotate the offset so the hexagon is tilted by orientationDegrees
                offset = glm::vec3(rotationMatrix * glm::vec4(offset, 0.0f));

                glm::vec3 vertex = center + offset;
                currentHexVertices.push_back(vertex);
                int idx = findOrAddVertex(vertex);
                hexVertexIndices.push_back(idx);
            }
            
            // Store this hexagon's vertices
            hexagonVertexLists.push_back(currentHexVertices);
            hexagonIndices.push_back(hexVertexIndices);

            // Create edges (each edge stored once using std::minmax)
            for (int i = 0; i < 6; ++i) {
                int idx1 = hexVertexIndices[i];
                int idx2 = hexVertexIndices[(i+1) % 6];
                edgeSet.insert(std::minmax(idx1, idx2));
            }
        }
    }

    // Create particles from unique positions
    for (const auto &pos : uniquePositions) {
        PMat* ball = new PMat(1.0f, pos);
        ballObjects.push_back(ball);
        Ball b;
        b.position = pos;
        b.color = glm::vec3(1.0f, 0.0f, 0.0f);
        b.type = ParticleType::STRUCTURE;
        b.dimensions = glm::vec3(3.0f);
        balls.push_back(b);
    }

    // Create springs from edges
    for (const auto &edge : edgeSet) {
        Spring* spring = new Spring(
            ballObjects[edge.first],
            ballObjects[edge.second],
            springConstant,
            springRestLength
        );
        spring->setDampingCoefficient(0.5f);
        springs.push_back(spring);
    }

    // Create collision faces (triangulate each hexagon)
    for (const auto& hexVertices : hexagonVertexLists) {
        if (hexVertices.size() != 6) continue;

        // Calculate center point of the hexagon
        glm::vec3 center(0.0f);
        for (const auto& v : hexVertices) {
            center += v;
        }
        center /= hexVertices.size();

        // Create 6 triangular faces (fan triangulation)
        for (int i = 0; i < 6; ++i) {
            int next = (i + 1) % 6;
            HexFace face;
            face.triangle = {center, hexVertices[i], hexVertices[next]};
            
            // Calculate face normal
            glm::vec3 edge1 = hexVertices[i] - center;
            glm::vec3 edge2 = hexVertices[next] - center;
            face.normal = glm::normalize(glm::cross(edge1, edge2));
            
            hexFaces.push_back(face);
        }
    }

    // Set static flags based on x-position (left/right particles)
    float minX = FLT_MAX;
    float maxX = -FLT_MAX;
    for (const auto &pos : uniquePositions) {
        if (pos.x < minX) minX = pos.x;
        if (pos.x > maxX) maxX = pos.x;
    }

    for (const auto &pos : uniquePositions) {
        bool isLeftStatic = pos.x <= minX + 0.001f;
        bool isRightStatic = bothEndsStatic && (pos.x >= maxX - 0.001f);
        ballStaticFlags.push_back(isLeftStatic || isRightStatic);
    }
}

void Simulation::createSquareGridWithDiagonals(int gridSize, float spacing, float springRestLength) {
    clearSimulation();
    ballStaticFlags.clear();
    int numRows = gridSize + 1;
    int numCols = gridSize + 1;
    std::vector<std::vector<int>> gridIndices(numRows, std::vector<int>(numCols, -1));

    for (int r = 0; r < numRows; ++r) {
        for (int c = 0; c < numCols; ++c) {
            glm::vec3 pos(c * spacing, r * spacing, 0.0f);
            PMat* ball = new PMat(10.0f, pos);
            ballObjects.push_back(ball);
            Ball b;
            b.position = pos;
            b.color = glm::vec3(1.0f, 0.0f, 0.0f);
            balls.push_back(b);
            ballStaticFlags.push_back(c == 0);
            gridIndices[r][c] = static_cast<int>(ballObjects.size() - 1);
        }
    }

    // Horizontal springs
    for (int r = 0; r < numRows; ++r)
        for (int c = 0; c < numCols - 1; ++c) {
            Spring* spring = new Spring(ballObjects[gridIndices[r][c]],
                                        ballObjects[gridIndices[r][c+1]],
                                        springConstant,
                                        springRestLength);
            spring->setDampingCoefficient(0.5f);
            springs.push_back(spring);
        }
    // Vertical springs
    for (int r = 0; r < numRows - 1; ++r)
        for (int c = 0; c < numCols; ++c) {
            Spring* spring = new Spring(ballObjects[gridIndices[r][c]],
                                        ballObjects[gridIndices[r+1][c]],
                                        springConstant,
                                        springRestLength);
            spring->setDampingCoefficient(0.5f);
            springs.push_back(spring);
        }
    // Diagonal springs (both diagonals per square)
    for (int r = 0; r < numRows - 1; ++r) {
        for (int c = 0; c < numCols - 1; ++c) {
            int idxTL = gridIndices[r][c];
            int idxTR = gridIndices[r][c+1];
            int idxBL = gridIndices[r+1][c];
            int idxBR = gridIndices[r+1][c+1];
            Spring* d1 = new Spring(ballObjects[idxTL], ballObjects[idxBR], springConstant, springRestLength);
            d1->setDampingCoefficient(0.5f);
            springs.push_back(d1);
            Spring* d2 = new Spring(ballObjects[idxTR], ballObjects[idxBL], springConstant, springRestLength);
            d2->setDampingCoefficient(0.5f);
            springs.push_back(d2);
        }
    }
}

void Simulation::collisionDetectionAndResolution(float dt) {
    const float collisionOffset = 0.1f; // Increased offset
    const float restitution = 0.8f;       // Bounce energy

    std::vector<PMat*> externalParticles;
    for (PMat* p : ballObjects) {
        if (p->type == ParticleType::EXTERNAL) {
            externalParticles.push_back(p);
        }
    }

    #pragma omp parallel for
    for (size_t i = 0; i < externalParticles.size(); ++i) {
        PMat* extP = externalParticles[i];
        glm::vec3 pos = extP->getPosition();
        glm::vec3 vel = extP->getVelocity();

        for (const HexFace& face : hexFaces) {
            glm::vec3 planePoint = face.triangle[0];
            glm::vec3 normal = face.normal;
            float dist = glm::dot(pos - planePoint, normal);

            // Only check front-facing collisions.
            if (dist > collisionOffset || dist < 0.0f)
                continue;

            if (isPointInTriangle(pos, face.triangle, normal)) {
                // Compute total correction impulse required to separate the objects.
                glm::vec3 totalCorrection = normal * (collisionOffset - dist) * impulseScaling;

                // Mass-based weighting:
                // Let m_external be the mass of the external particle.
                float m_external = extP->getMass();
                // Retrieve the indices for the hexagon that produced this face.
                const std::vector<int>& indices = hexagonIndices[face.hexagonIndex];
                // Compute total mass of the structure particles involved.
                float m_structure = 0.0f;
                for (int idx : indices) {
                    m_structure += ballObjects[idx]->getMass();
                }
                float totalMass = m_external + m_structure;
                if(totalMass < 1e-6f)
                    totalMass = 1e-6f;
                // Define mass-based fractions:
                float extFraction = m_structure / totalMass;    // External particle gets a fraction proportional to the structure mass.
                float structFraction = m_external / totalMass;    // Structure gets a fraction proportional to the external mass.

                // Apply correction:
                extP->addCorrection(totalCorrection * extFraction);

                // Distribute the remaining correction among the structure particles.
                glm::vec3 structureCorrection = -(totalCorrection * structFraction) / static_cast<float>(indices.size());
                for (int idx : indices) {
                    ballObjects[idx]->addCorrection(structureCorrection);

                    // Reflect velocity for structure particles.
                    glm::vec3 structVel = ballObjects[idx]->getVelocity();
                    float velDot = glm::dot(structVel, normal);
                    if (velDot < 0) {
                        glm::vec3 newVel = structVel - (1.0f + restitution) * velDot * normal;
                        ballObjects[idx]->reflectVelocity(newVel);
                    }
                }

                // Also reflect velocity for the external particle.
                float extVelDot = glm::dot(vel, normal);
                if (extVelDot < 0) {
                    glm::vec3 reflected = vel - (1.0f + restitution) * extVelDot * normal;
                    extP->reflectVelocity(reflected);
                }
            }
        }
    }
}

bool Simulation::isPointInTriangle(const glm::vec3& point, 
                                  const std::array<glm::vec3, 3>& triangle,
                                  const glm::vec3& normal) const {
    const float epsilon = 1e-6f;
    
    glm::vec3 edge0 = triangle[1] - triangle[0];
    glm::vec3 edge1 = triangle[2] - triangle[0];
    glm::vec3 vec = point - triangle[0];
    
    float d00 = glm::dot(edge0, edge0);
    float d01 = glm::dot(edge0, edge1);
    float d11 = glm::dot(edge1, edge1);
    float d20 = glm::dot(vec, edge0);
    float d21 = glm::dot(vec, edge1);
    float denom = d00 * d11 - d01 * d01;
    
    if (fabs(denom) < epsilon) return false;
    
    float u = (d11 * d20 - d01 * d21) / denom;
    float v = (d00 * d21 - d01 * d20) / denom;
    
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

void Simulation::throwBall(const glm::vec3& cameraPos, const glm::vec3& cameraDir,
                             float speed, float mass, const glm::vec3& dimensions) {
    glm::vec3 spawnPos = cameraPos + cameraDir * 1.0f;
    glm::vec3 initialVelocity = cameraDir * speed;
    BallParticle* ball = new BallParticle(mass, spawnPos, initialVelocity, dimensions, ParticleType::EXTERNAL);
    ballObjects.push_back(ball);
    Ball b;
    b.position = ball->getPosition();
    b.color = glm::vec3(0.0f, 1.0f, 0.0f);
    b.type = ball->type;
    b.dimensions = ball->getDimensions();
    balls.push_back(b);
    ballStaticFlags.push_back(false);
}

void Simulation::applyGravityLink() {
    // Apply gravity to all particles in the system using the gravity link
    gravityLink->applyGravity();
}

std::vector<glm::vec3> Simulation::getParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> positions;
    for (const auto* ball : ballObjects) {
        positions.push_back(ball->getPosition());
    }
    return positions;
}

std::vector<glm::vec3> Simulation::getStructureParticlePositions() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> positions;
    for (const auto* ball : ballObjects) {
        if (ball->type == ParticleType::STRUCTURE) {
            positions.push_back(ball->getPosition());
        }
    }
    return positions;
}

const std::vector<glm::vec3> Simulation::getParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> velocities;
    for (const auto& ball : ballObjects) {
        velocities.push_back(ball->getVelocity());
    }
    return velocities;
}

const std::vector<glm::vec3> Simulation::getStructureParticleVelocities() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    std::vector<glm::vec3> velocities;
    for (const auto* ball : ballObjects) {
        if (ball->type == ParticleType::STRUCTURE) {
            velocities.push_back(ball->getVelocity());
        }
    }
    return velocities;
}

std::vector<glm::vec3> Simulation::getSpringEndpoints() const {
    std::vector<glm::vec3> endpoints;
    for (const Spring* spring : springs) {
        endpoints.push_back(spring->getP1()->getPosition());
        endpoints.push_back(spring->getP2()->getPosition());
    }
    return endpoints;
}

void Simulation::clearSimulation() {
    std::cout << "Clearing simulation..." << std::endl;

    for (Spring* spring : springs) {
        // std::cout << "Deleting Spring: " << spring << std::endl;
        delete spring;
    }
    springs.clear();

    for (PMat* ball : ballObjects) {
        if (dynamic_cast<BallParticle*>(ball)) {
            // std::cout << "Deleting BallParticle: " << ball << std::endl;
        } else {
            // std::cout << "Deleting PMat: " << ball << std::endl;
        }
    }

    for (PMat* ball : ballObjects) {
        if (ball) { // Only delete if it's not already deleted
            // std::cout << "Deleting PMat: " << ball << std::endl;
            delete ball;
            ball = nullptr;
        }
    }
    ballObjects.clear();

    hexagonVertexLists.clear();
    hexFaces.clear();

    balls.clear();
    ballStaticFlags.clear();  // <-- Clear the static flags as well.

    if (gravityLink) {
        // std::cout << "Deleting Gravity Link: " << gravityLink << std::endl;
        delete gravityLink;
        gravityLink = nullptr;
    }
}

// get balls
const std::vector<Ball>& Simulation::getBalls() const {
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);
    return balls;
}

std::vector<glm::vec3> Simulation::getHexHitboxTriangles() const {
    std::vector<glm::vec3> vertices;
    // Each hex face is represented by 3 vertices (a triangle)
    for (const auto& face : hexFaces) {
        for (int i = 0; i < 3; ++i) {
            vertices.push_back(face.triangle[i]);
        }
    }
    return vertices;
}