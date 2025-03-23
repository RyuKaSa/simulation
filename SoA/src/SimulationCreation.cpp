// SimulationCreation.cpp

#include "Simulation.hpp"
#include <set>
#include <utility>
#include <algorithm>
#include <iostream>
#include <cmath>

// A static clothID counter, if you're using clothID in your SoA
static int nextClothID = 0;

// ------------------ addStaticCubeUnderGrid ------------------ //
// Creates an external cube with clothID = -1 (not part of a cloth).
void SimulationBase::addStaticCubeUnderGrid()
{
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    glm::dvec3 center(0.0);
    int count = 0;
    for (size_t i = 0; i < soA.position.size(); i++) {
        if (soA.type[i] == ParticleType::STRUCTURE) {
            center += soA.position[i];
            count++;
        }
    }
    if (count > 0) {
        center /= static_cast<double>(count);
    } else {
        center = glm::dvec3(0.0);
    }
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

    // External block => clothID = -1
    soA.clothID.push_back(-1);
}

// ------------------ createCord ------------------ //
void SimulationBase::createCord(int numBalls, double length,
                                double springRestLength, bool bothEndsStatic)
{
    // Do not clear the simulation; we append a new cloth.
    size_t startIndex = soA.position.size();
    int clothID = nextClothID++;

    double spacing = length / (numBalls - 1);
    glm::dvec3 startPos(-length / 2.0, 0.0, 0.0);

    soA.position.reserve(startIndex + numBalls);
    soA.velocity.reserve(startIndex + numBalls);
    soA.forceAccum.reserve(startIndex + numBalls);
    soA.mass.reserve(startIndex + numBalls);
    soA.type.reserve(startIndex + numBalls);
    soA.isStatic.reserve(startIndex + numBalls);
    soA.color.reserve(startIndex + numBalls);
    soA.dimensions.reserve(startIndex + numBalls);
    soA.clothID.reserve(startIndex + numBalls);

    for (int i = 0; i < numBalls; i++) {
        glm::dvec3 pos = startPos + glm::dvec3(i * spacing, 0.0, 0.0);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::dvec3(0.0));
        soA.forceAccum.push_back(glm::dvec3(0.0));
        soA.mass.push_back(10.0);
        soA.type.push_back(ParticleType::STRUCTURE);

        bool sflag = (bothEndsStatic) ? (i == 0 || i == (numBalls - 1)) : (i == 0);
        soA.isStatic.push_back(sflag);

        soA.color.push_back(glm::dvec3(1.0, 0.0, 0.0));
        soA.dimensions.push_back(glm::dvec3(3.0));
        soA.clothID.push_back(clothID);
    }

    // Create springs
    for (int i = 0; i < numBalls - 1; i++) {
        size_t idxA = startIndex + i;
        size_t idxB = startIndex + i + 1;

        double dist = glm::distance(soA.position[idxA], soA.position[idxB]);
        SpringData sp;
        sp.p1Index = (int)idxA;
        sp.p2Index = (int)idxB;
        sp.restLength = dist * springRestLength;
        sp.springConstant = sharedParams.springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

// ------------------ createSquareGridWithDiagonals ------------------ //
void SimulationBase::createSquareGridWithDiagonals(int gridSize,
                                                   double cellSize,
                                                   double springRestLength)
{
    size_t startIndex = soA.position.size();
    int clothID = nextClothID++;

    int numParticles = gridSize * gridSize;
    soA.position.resize(startIndex + numParticles);
    soA.velocity.resize(startIndex + numParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(startIndex + numParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(startIndex + numParticles, massValue);
    soA.type.resize(startIndex + numParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(startIndex + numParticles, false);
    soA.color.resize(startIndex + numParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(startIndex + numParticles, glm::dvec3(1.0));
    soA.clothID.resize(startIndex + numParticles, clothID);

    double halfWidth = (gridSize - 1) * cellSize / 2.0;
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            int localIndex = i * gridSize + j;
            size_t idx = startIndex + localIndex;
            soA.position[idx] =
                glm::dvec3(j*cellSize - halfWidth, 0.0, i*cellSize - halfWidth);
        }
    }

    // Left & right edges static
    for (int i = 0; i < gridSize; i++) {
        size_t leftIdx  = startIndex + i*gridSize;
        size_t rightIdx = startIndex + i*gridSize + (gridSize - 1);
        soA.isStatic[leftIdx]  = true;
        soA.isStatic[rightIdx] = true;
    }

    // Springs horizontally, vertically, diagonally
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++) {
            size_t idx = startIndex + (i*gridSize + j);

            // Right neighbor
            if (j < gridSize - 1) {
                size_t rightIdx = startIndex + (i*gridSize + (j+1));
                double dist = glm::distance(soA.position[idx], soA.position[rightIdx]);
                SpringData sp;
                sp.p1Index = (int)idx;
                sp.p2Index = (int)rightIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            // Down neighbor
            if (i < gridSize - 1) {
                size_t bottomIdx = startIndex + ((i+1)*gridSize + j);
                double dist = glm::distance(soA.position[idx], soA.position[bottomIdx]);
                SpringData sp;
                sp.p1Index = (int)idx;
                sp.p2Index = (int)bottomIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            // Diagonal
            if (i < gridSize - 1 && j < gridSize - 1) {
                size_t diagIdx = startIndex + ((i+1)*gridSize + (j+1));
                double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                SpringData sp;
                sp.p1Index = (int)idx;
                sp.p2Index = (int)diagIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            if (i < gridSize - 1 && j > 0) {
                size_t diagIdx = startIndex + ((i+1)*gridSize + (j-1));
                double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                SpringData sp;
                sp.p1Index = (int)idx;
                sp.p2Index = (int)diagIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
        }
    }
}

// ------------------ createMultiLayerSquareGridWithDiagonals ------------------ //
void SimulationBase::createMultiLayerSquareGridWithDiagonals(int gridSize,
                                                             int nLayers,
                                                             double cellSize,
                                                             double layerSpacing,
                                                             double springRestLength)
{
    size_t startIndex = soA.position.size();
    int clothID = nextClothID++;

    int particlesPerLayer = gridSize * gridSize;
    int totalParticles    = nLayers * particlesPerLayer;

    soA.position.resize(startIndex + totalParticles);
    soA.velocity.resize(startIndex + totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(startIndex + totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(startIndex + totalParticles, massValue);
    soA.type.resize(startIndex + totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(startIndex + totalParticles, false);
    soA.color.resize(startIndex + totalParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(startIndex + totalParticles, glm::dvec3(1.0));
    soA.clothID.resize(startIndex + totalParticles, clothID);

    double halfWidth = (gridSize - 1) * cellSize / 2.0;

    // Position each layer
    for (int l = 0; l < nLayers; l++) {
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int localIndex = l * particlesPerLayer + i * gridSize + j;
                size_t idx     = startIndex + localIndex;

                double x = j * cellSize - halfWidth;
                double z = i * cellSize - halfWidth;
                double y = l * layerSpacing;
                soA.position[idx] = glm::dvec3(x, y, z);
            }
        }
    }

    // Left & right edges static in each layer
    for (int l = 0; l < nLayers; l++) {
        for (int i = 0; i < gridSize; i++) {
            size_t leftIdx  = startIndex + (l*particlesPerLayer) + (i*gridSize);
            size_t rightIdx = startIndex + (l*particlesPerLayer) + (i*gridSize + (gridSize - 1));
            soA.isStatic[leftIdx]  = true;
            soA.isStatic[rightIdx] = true;
        }
    }

    // Springs within each layer
    for (int l = 0; l < nLayers; l++) {
        int layerOffset = startIndex + (l*particlesPerLayer);

        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int localIndex = i * gridSize + j;
                size_t idx = layerOffset + localIndex;

                // Right neighbor
                if (j < gridSize - 1) {
                    size_t rightIdx = layerOffset + (i*gridSize + (j+1));
                    double dist = glm::distance(soA.position[idx], soA.position[rightIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)rightIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                // Down neighbor
                if (i < gridSize - 1) {
                    size_t bottomIdx = layerOffset + ((i+1)*gridSize + j);
                    double dist = glm::distance(soA.position[idx], soA.position[bottomIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)bottomIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                // Diagonal
                if (i < gridSize - 1 && j < gridSize - 1) {
                    size_t diagIdx = layerOffset + ((i+1)*gridSize + (j+1));
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                if (i < gridSize - 1 && j > 0) {
                    size_t diagIdx = layerOffset + ((i+1)*gridSize + (j-1));
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
            }
        }
    }

    // Springs between layers
    for (int l = 0; l < nLayers - 1; l++) {
        int lowerOffset = startIndex + (l     * particlesPerLayer);
        int upperOffset = startIndex + ((l+1) * particlesPerLayer);

        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                size_t lowerIdx = lowerOffset + (i*gridSize + j);
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni < 0 || ni >= gridSize) continue;
                        if (nj < 0 || nj >= gridSize) continue;

                        size_t upperIdx = upperOffset + (ni*gridSize + nj);
                        double dist = glm::distance(soA.position[lowerIdx],
                                                    soA.position[upperIdx]);
                        SpringData sp;
                        sp.p1Index = (int)lowerIdx;
                        sp.p2Index = (int)upperIdx;
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

// ------------------ createHexGrid ------------------ //
void SimulationBase::createHexGrid(int numHexagons,
                                   double hexagonSize,
                                   double springRestLength,
                                   bool bothEndsStatic,
                                   double orientationDegrees)
{
    // We do not clear, so we can accumulate multiple cloths.
    springs.clear();
    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    std::vector<glm::dvec3> uniquePositions;
    std::set<std::pair<int,int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix,
                         uniquePositions, edgeSet);

    size_t oldCount = soA.position.size();
    size_t newCount = uniquePositions.size();
    int clothID = nextClothID++;

    soA.position.resize(oldCount + newCount);
    soA.velocity.resize(oldCount + newCount, glm::dvec3(0.0));
    soA.forceAccum.resize(oldCount + newCount, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(oldCount + newCount, massValue);
    soA.type.resize(oldCount + newCount, ParticleType::STRUCTURE);
    soA.isStatic.resize(oldCount + newCount, false);
    soA.color.resize(oldCount + newCount, glm::dvec3(1.0, 0.4, 0.0));
    soA.dimensions.resize(oldCount + newCount, glm::dvec3(1.0));
    soA.clothID.resize(oldCount + newCount, clothID);

    double minX = 1e9, maxX = -1e9;
    for (auto &p : uniquePositions) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }

    for (size_t i = 0; i < newCount; i++) {
        size_t idx = oldCount + i;
        soA.position[idx] = uniquePositions[i];
    }
    for (size_t i = 0; i < newCount; i++) {
        size_t idx = oldCount + i;
        double px = soA.position[idx].x;
        bool leftStatic  = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[idx] = (leftStatic || rightStatic);
    }

    // Now create springs from edgeSet
    for (auto &e : edgeSet) {
        int i1 = e.first  + (int)oldCount;
        int i2 = e.second + (int)oldCount;
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

// ------------------ createMultiLayerHexGrid ------------------ //
void SimulationBase::createMultiLayerHexGrid(int numHexagons,
                                             double hexagonSize,
                                             double springRestLength,
                                             bool bothEndsStatic,
                                             double orientationDegrees,
                                             double layerHeight,
                                             int nLayers)
{
    // We do not call clearSimulation so we can accumulate multiple cloths.
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
    size_t oldCount  = soA.position.size();
    size_t total     = nLayers * baseCount;
    int clothID      = nextClothID++;

    soA.position.resize(oldCount + total);
    soA.velocity.resize(oldCount + total, glm::dvec3(0.0));
    soA.forceAccum.resize(oldCount + total, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(oldCount + total, massValue);
    soA.type.resize(oldCount + total, ParticleType::STRUCTURE);
    soA.isStatic.resize(oldCount + total, false);
    soA.color.resize(oldCount + total, glm::dvec3(1.0));
    soA.dimensions.resize(oldCount + total, glm::dvec3(2.0));
    soA.clothID.resize(oldCount + total, clothID);

    glm::dvec3 minPos = baseUniquePositions[0];
    glm::dvec3 maxPos = baseUniquePositions[0];
    for (size_t i = 1; i < baseCount; i++) {
        minPos = glm::min(minPos, baseUniquePositions[i]);
        maxPos = glm::max(maxPos, baseUniquePositions[i]);
    }

    // Layout the layers
    for (int layer = 0; layer < nLayers; layer++) {
        for (size_t i = 0; i < baseCount; i++) {
            size_t idx = oldCount + (layer*baseCount) + i;
            glm::dvec3 pos = baseUniquePositions[i];
            pos.y += (layerHeight * layer);
            soA.position[idx] = pos;
        }
    }

    double minX = minPos.x;
    double maxX = maxPos.x;
    // Mark static if near left or right
    for (int layer = 0; layer < nLayers; layer++) {
        for (size_t i = 0; i < baseCount; i++) {
            size_t idx = oldCount + (layer*baseCount) + i;
            double px = soA.position[idx].x;
            bool leftStatic  = (px <= minX + 0.001);
            bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
            if (leftStatic || rightStatic) {
                soA.isStatic[idx] = true;
            }
        }
    }

    // Springs within each layer
    for (int layer = 0; layer < nLayers; layer++) {
        size_t layerOffset = oldCount + (layer*baseCount);
        for (auto &edge : edgeSet) {
            int i1 = edge.first  + (int)layerOffset;
            int i2 = edge.second + (int)layerOffset;
            double dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index    = i1;
            sp.p2Index    = i2;
            sp.restLength = dist * springRestLength;
            sp.springConstant = sharedParams.springConstant;
            sp.damping    = 0.5;
            springs.push_back(sp);
        }
    }

    // If you want to connect layers, do it here. For example, you can connect
    // each vertex in layer L to nearby vertices in layer L+1. If not, you can skip:
    // (Here we skip, so each layer is separate.)
}

// ------------------ createRotationMatrix ------------------ //
glm::dmat4 SimulationBase::createRotationMatrix(double orientationDegrees)
{
    return glm::rotate(glm::dmat4(1.0),
                       glm::radians(orientationDegrees),
                       glm::dvec3(1.0, 0.0, 0.0));
}

// ------------------ generateHexagonCells ------------------ //
void SimulationBase::generateHexagonCells(int numHexagons,
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
        return (int)uniquePositions.size() - 1;
    };

    // Simple hex layout
    for (int r = 0; r < numHexagons; r++) {
        for (int c = 0; c < numHexagons; c++) {
            glm::dvec3 center;
            center.x = std::sqrt(3.0) * hexagonSize * (c + (r % 2) * 0.5);
            center.y = 1.5 * hexagonSize * r;
            center.z = 0.0;
            center = glm::dvec3(rotationMatrix * glm::dvec4(center, 1.0));

            std::vector<int> indices;
            indices.reserve(6);
            for (int i = 0; i < 6; i++) {
                double ang = glm::radians(60.0 * i + 90.0);
                glm::dvec3 offset(hexagonSize * std::cos(ang),
                                  hexagonSize * std::sin(ang),
                                  0.0);
                offset = glm::dvec3(rotationMatrix * glm::dvec4(offset, 0.0));
                glm::dvec3 vertex = center + offset;
                int idx = findOrAdd(vertex);
                indices.push_back(idx);
            }
            // Build edges
            for (int i = 0; i < 6; i++) {
                int i1 = indices[i];
                int i2 = indices[(i+1) % 6];
                if (i1 > i2) std::swap(i1, i2);
                edgeSet.insert({ i1, i2 });
            }
        }
    }
}

// ------------------ Legacy functions if needed -------------- //
void SimulationBase::assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions,
                                                bool bothEndsStatic)
{
    // This is leftover from your original code. If you want each new
    // set of unique positions to be appended with a clothID, do so here as well.
    // Otherwise, you can skip usage of this function.
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

    // If you want these to have a clothID, do it:
    // int clothID = nextClothID++;
    // soA.clothID.resize(n, clothID);

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
        bool leftStatic  = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[i]  = (leftStatic || rightStatic);
    }
}

void SimulationBase::createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet,
                                              double springRestLength)
{
    // If you do not offset indices (like oldCount), this function must assume
    // the SoA is exactly sized for those edges. Typically used in your older code.
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

bool SimulationBase::approxEqualVec3(const glm::dvec3& a,
                                     const glm::dvec3& b,
                                     double epsilon)
{
    return (std::fabs(a.x - b.x) < epsilon) &&
           (std::fabs(a.y - b.y) < epsilon) &&
           (std::fabs(a.z - b.z) < epsilon);
}

int SimulationBase::findApproxVertexIndex(const std::vector<glm::dvec3>& vertices,
                                          const glm::dvec3& target,
                                          double epsilon)
{
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (approxEqualVec3(vertices[i], target, epsilon)) {
            return (int)i;
        }
    }
    return -1;
}

void SimulationBase::createMultiLayerSquareGridWithDiagonalsCentered(
    int cx, int cy, int cz,      // Grid coordinate of the block center
    int gridSize,              // Number of particles per row/column in each layer
    int nLayers,               // Number of layers in the cloth (vertical stack)
    double cellSize,           // Spacing between cloth particles in a layer
    double layerSpacing,       // Spacing between layers
    double springRestLength)   // Factor to scale the computed rest lengths of springs
{
    // Determine starting index and assign a new cloth ID.
    size_t startIndex = soA.position.size();
    int clothID = nextClothID++;  // nextClothID is assumed to be a member variable

    int particlesPerLayer = gridSize * gridSize;
    int totalParticles = nLayers * particlesPerLayer;

    // Resize the SoA vectors to accommodate the new cloth particles.
    soA.position.resize(startIndex + totalParticles);
    soA.velocity.resize(startIndex + totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(startIndex + totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(startIndex + totalParticles, massValue);
    soA.type.resize(startIndex + totalParticles, ParticleType::STRUCTURE);
    // All particles are dynamic (non-static):
    soA.isStatic.resize(startIndex + totalParticles, false);
    soA.color.resize(startIndex + totalParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(startIndex + totalParticles, glm::dvec3(1.0));
    soA.clothID.resize(startIndex + totalParticles, clothID);

    // Compute the world-space center for the target block.
    // (Assuming gridToWorld returns the center of the grid cell.)
    glm::dvec3 blockCenter = glm::dvec3(cx, cy, cz);

    // Compute half-width so that the cloth is centered on the block.
    double halfWidth = (gridSize - 1) * cellSize / 2.0;

    // --- Position particles for each layer ---
    // The (x,z) positions span from -halfWidth to +halfWidth,
    // and the vertical (y) positions are offset by layerSpacing.
    for (int l = 0; l < nLayers; l++) {
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int localIndex = l * particlesPerLayer + i * gridSize + j;
                size_t idx = startIndex + localIndex;

                double x = j * cellSize - halfWidth;
                double z = i * cellSize - halfWidth;
                double y = l * layerSpacing;
                soA.position[idx] = blockCenter + glm::dvec3(x, y, z);
            }
        }
    }

    // --- Create springs within each layer ---
    for (int l = 0; l < nLayers; l++) {
        int layerOffset = startIndex + (l * particlesPerLayer);
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int localIndex = i * gridSize + j;
                size_t idx = layerOffset + localIndex;

                // Right neighbor
                if (j < gridSize - 1) {
                    size_t rightIdx = layerOffset + (i * gridSize + (j + 1));
                    double dist = glm::distance(soA.position[idx], soA.position[rightIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)rightIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                // Down neighbor
                if (i < gridSize - 1) {
                    size_t bottomIdx = layerOffset + ((i + 1) * gridSize + j);
                    double dist = glm::distance(soA.position[idx], soA.position[bottomIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)bottomIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                // Diagonal: down-right
                if (i < gridSize - 1 && j < gridSize - 1) {
                    size_t diagIdx = layerOffset + ((i + 1) * gridSize + (j + 1));
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
                // Diagonal: down-left
                if (i < gridSize - 1 && j > 0) {
                    size_t diagIdx = layerOffset + ((i + 1) * gridSize + (j - 1));
                    double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                    SpringData sp;
                    sp.p1Index = (int)idx;
                    sp.p2Index = (int)diagIdx;
                    sp.restLength = dist * springRestLength;
                    sp.springConstant = sharedParams.springConstant;
                    sp.damping = 0.5;
                    springs.push_back(sp);
                }
            }
        }
    }

    // --- Create springs between layers ---
    for (int l = 0; l < nLayers - 1; l++) {
        int lowerOffset = startIndex + (l * particlesPerLayer);
        int upperOffset = startIndex + ((l + 1) * particlesPerLayer);
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                size_t lowerIdx = lowerOffset + (i * gridSize + j);
                // Connect each particle to its adjacent neighbors in the layer above.
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni < 0 || ni >= gridSize) continue;
                        if (nj < 0 || nj >= gridSize) continue;
                        size_t upperIdx = upperOffset + (ni * gridSize + nj);
                        double dist = glm::distance(soA.position[lowerIdx], soA.position[upperIdx]);
                        SpringData sp;
                        sp.p1Index = (int)lowerIdx;
                        sp.p2Index = (int)upperIdx;
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