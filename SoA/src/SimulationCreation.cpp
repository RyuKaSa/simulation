#include "Simulation.hpp"
#include <set>
#include <utility>
#include <algorithm>
#include <iostream>

// --- Creation Functions ---

void Simulation::addStaticCubeUnderGrid() {
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
    double cubeSize = 80.0;
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

void Simulation::createCord(int numBalls, double length, double springRestLength, bool bothEndsStatic_) {
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
    glm::dvec3 startPos(-length / 2, 0.0, 0.0);

    for (int i = 0; i < numBalls; i++) {
        glm::dvec3 pos = startPos + glm::dvec3(i * spacing, 0.0, 0.0);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::dvec3(0.0));
        soA.forceAccum.push_back(glm::dvec3(0.0));
        soA.mass.push_back(10.0);
        soA.type.push_back(ParticleType::STRUCTURE);
        bool sflag = (bothEndsStatic_) ? (i == 0 || i == (numBalls - 1)) : (i == 0);
        soA.isStatic.push_back(sflag);
        soA.color.push_back(glm::dvec3(1, 0, 0));
        soA.dimensions.push_back(glm::dvec3(3.0));
    }
    for (int i = 0; i < numBalls - 1; i++) {
        double dist = glm::distance(soA.position[i], soA.position[i + 1]);
        SpringData sp;
        sp.p1Index = i;
        sp.p2Index = i + 1;
        sp.restLength = dist * springRestLength;
        sp.springConstant = springConstant;
        sp.damping = 0.5;
        springs.push_back(sp);
    }
}

void Simulation::createSquareGridWithDiagonals(int gridSize, double cellSize, double springRestLength) {
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
                sp.springConstant = springConstant;
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
                sp.springConstant = springConstant;
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
                sp.springConstant = springConstant;
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
                sp.springConstant = springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
        }
    }
}

void Simulation::createMultiLayerSquareGridWithDiagonals(int gridSize, int nLayers, double cellSize, double layerSpacing, double springRestLength) {
    clearSimulation();

    int particlesPerLayer = gridSize * gridSize;
    int totalParticles = nLayers * particlesPerLayer;
    soA.position.resize(totalParticles);
    soA.velocity.resize(totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    // check with std if gui instance is not null
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
                    sp.springConstant = springConstant;
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
                    sp.springConstant = springConstant;
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
                    sp.springConstant = springConstant;
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
                    sp.springConstant = springConstant;
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
                        sp.springConstant = springConstant;
                        sp.damping = 0.5;
                        springs.push_back(sp);
                    }
                }
            }
        }
    }
}

void Simulation::createHexGrid(int numHexagons, double hexagonSize, double springRestLength, bool bothEndsStatic_, double orientationDegrees) {
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
    // Optionally call updateHexTriangles() if desired.
}

void Simulation::createMultiLayerHexGrid(int numHexagons, double hexagonSize, double springRestLength, bool bothEndsStatic, double orientationDegrees, double layerHeight, int nLayers) {
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
            sp.springConstant = springConstant;
            sp.damping = 0.5;
            springs.push_back(sp);
        }
    }

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

                    if (approxEqualVec3(glm::vec3(soA.color[lowerGlobalIndex]), glm::vec3(1.0f, 0.0f, 0.0f))) {
                        lowerGreenIndices.push_back(lowerGlobalIndex);
                        lowerGreenPositions.push_back(glm::vec3(soA.position[lowerGlobalIndex]));
                    }
                    if (approxEqualVec3(glm::vec3(soA.color[upperGlobalIndex]), glm::vec3(0.0f, 1.0f, 0.0f))) {
                        upperRedIndices.push_back(upperGlobalIndex);
                        upperRedPositions.push_back(glm::vec3(soA.position[upperGlobalIndex]));
                    }
                }

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

glm::dmat4 Simulation::createRotationMatrix(double orientationDegrees) {
    return glm::rotate(glm::dmat4(1.0), glm::radians(orientationDegrees), glm::dvec3(1.0, 0.0, 0.0));
}

void Simulation::generateHexagonCells(int numHexagons, double hexagonSize, const glm::dmat4& rotationMatrix,
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

void Simulation::assignUniquePositionsToSoA(const std::vector<glm::dvec3>& uniquePositions, bool bothEndsStatic) {
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

void Simulation::createSpringsFromEdgeSet(const std::set<std::pair<int,int>>& edgeSet, double springRestLength) {
    for (auto &e : edgeSet) {
        int i1 = e.first;
        int i2 = e.second;
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
    return -1;
}