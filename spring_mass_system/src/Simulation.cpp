#include "Simulation.hpp"

Simulation::Simulation() : springConstant(0.5f) {
    Simulation::Initialization();
}

void Simulation::Initialization() {
    // createCord(50, 25.0f, 0.5f, false); // Default cord setup (3 balls, 1m length, both ends static)
    createHexGrid(100, 0.5f, 1.0f); // Default hex grid setup (5 hexagons, 1m hexagon size)
    // createSquareGridWithDiagonals(50, 0.5f, 1.0f); // Default square grid setup (10x10 grid
    // gravityLink = new Link(ballObjects, glm::vec3(0.0f, -9.81f, 0.0f));
    gravityLink = new Link(ballObjects, glm::vec3(5.0f, -9.81f, -1.0f));
}

// reset function, call clear, then call simulation
void Simulation::reset() {
    std::cout << "RESET initialized" << std::endl;
    Simulation::Initialization();
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
    
    // --- Parallel Particle Update ---
    size_t numParticles = ballObjects.size();
    size_t chunkSizeParticles = (numParticles + numThreads - 1) / numThreads;
    threads.clear();
    for (unsigned int t = 0; t < numThreads; t++) {
        size_t start = t * chunkSizeParticles;
        size_t end = std::min(start + chunkSizeParticles, numParticles);
        threads.push_back(std::thread([this, dt, start, end]() {
            for (size_t i = start; i < end; i++) {
                bool isStatic = (ballStaticFlags.size() == ballObjects.size())
                                  ? ballStaticFlags[i]
                                  : (bothEndsStatic ? (i == 0 || i == ballObjects.size()-1) : (i == 0));
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

void Simulation::createCord(int numBalls, float length, float springRestLength, bool bothEndsStatic) {
    clearSimulation();
    this->bothEndsStatic = bothEndsStatic;
    float spacing = length / (numBalls - 1);
    glm::vec3 startPos(-length / 2, 0.0f, 0.0f);

    for (int i = 0; i < numBalls; i++) {
        bool isStatic = bothEndsStatic ? (i == 0 || i == numBalls - 1) : (i == 0);
        PMat* ball = new PMat(1.0f, startPos + glm::vec3(i * spacing, 0.0f, 0.0f));
        ballObjects.push_back(ball);
        Ball b;
        b.position = ball->getPosition();
        b.color = glm::vec3(1.0f, 0.0f, 0.0f);
        balls.push_back(b);
    }

    for (int i = 0; i < numBalls - 1; i++) {
        Spring* spring = new Spring(ballObjects[i], ballObjects[i + 1], springConstant, springRestLength);
        spring->setDampingCoefficient(0.5f);
        springs.push_back(spring);
    }

}

void Simulation::createHexGrid(int numHexagons, float hexagonSize, float springRestLength) {
    clearSimulation();
    ballStaticFlags.clear();
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

    for (int r = 0; r < numHexagons; ++r) {
        for (int c = 0; c < numHexagons; ++c) {
            glm::vec3 center;
            center.x = sqrt(3.0f) * hexagonSize * (c + (r % 2) * 0.5f);
            center.y = 1.5f * hexagonSize * r;
            center.z = 0.0f;

            std::vector<int> hexVertexIndices;
            for (int i = 0; i < 6; ++i) {
                float angle = glm::radians(60.0f * i+ 90.0f);
                glm::vec3 vertex = center + glm::vec3(hexagonSize * cos(angle),
                                                       hexagonSize * sin(angle),
                                                       0.0f);
                int idx = findOrAddVertex(vertex);
                hexVertexIndices.push_back(idx);
            }
            for (int i = 0; i < 6; ++i) {
                int idx1 = hexVertexIndices[i];
                int idx2 = hexVertexIndices[(i+1) % 6];
                edgeSet.insert(std::minmax(idx1, idx2));
            }
            // Add interior diagonals (3 more springs per hexagon)
            // edgeSet.insert(std::minmax(hexVertexIndices[0], hexVertexIndices[3]));
            // edgeSet.insert(std::minmax(hexVertexIndices[1], hexVertexIndices[4]));
            // edgeSet.insert(std::minmax(hexVertexIndices[2], hexVertexIndices[5]));
        }
    }

    for (const auto &pos : uniquePositions) {
        PMat* ball = new PMat(1.0f, pos);
        ballObjects.push_back(ball);
        Ball b;
        b.position = pos;
        b.color = glm::vec3(1.0f, 0.0f, 0.0f);
        balls.push_back(b);
    }
    float minX = FLT_MAX;
    for (const auto &pos : uniquePositions)
        if (pos.x < minX) minX = pos.x;
    for (const auto &pos : uniquePositions)
        ballStaticFlags.push_back(pos.x <= minX + 0.001f);

    for (const auto &edge : edgeSet) {
        Spring* spring = new Spring(ballObjects[edge.first],
                                    ballObjects[edge.second],
                                    springConstant,
                                    springRestLength);
        spring->setDampingCoefficient(0.5f);
        springs.push_back(spring);
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
            PMat* ball = new PMat(1.0f, pos);
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

void Simulation::applyGravityLink() {
    // Apply gravity to all particles in the system using the gravity link
    gravityLink->applyGravity();
}

std::vector<glm::vec3> Simulation::getParticlePositions() const {
    std::vector<glm::vec3> positions;
    for (const auto* ball : ballObjects) {
        positions.push_back(ball->getPosition());
    }
    return positions;
}

const std::vector<glm::vec3> Simulation::getParticleVelocities() const {
    std::vector<glm::vec3> velocities;
    for (const auto& ball : ballObjects) {
        velocities.push_back(ball->getVelocity());
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
    for (Spring* spring : springs) {
        delete spring;
    }
    springs.clear();

    for (PMat* ball : ballObjects) {
        delete ball;
    }
    ballObjects.clear();

    balls.clear();

    if (gravityLink) {
        delete gravityLink;
        gravityLink = nullptr;
    }
}

// get balls
const std::vector<Ball>& Simulation::getBalls() const {
    return balls;
}