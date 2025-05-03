// SimulationCreation.cpp

#include "Simulation.hpp"
#include <set>
#include <utility>
#include <algorithm>
#include <iostream>
#include <cmath>

static int nextClothID = 0;

// ------------------ addStaticCubeUnderGrid ------------------ //
// Creates an external cube with clothID = -1 (not part of a cloth).
void SimulationBase::addStaticCubeUnderGrid()
{
    std::lock_guard<std::recursive_mutex> lock(simulationMutex);

    glm::dvec3 center(0.0);
    int count = 0;
    for (size_t i = 0; i < soA.position.size(); i++)
    {
        if (soA.type[i] == ParticleType::STRUCTURE)
        {
            center += soA.position[i];
            count++;
        }
    }
    if (count > 0)
    {
        center /= static_cast<double>(count);
    }
    else
    {
        center = glm::dvec3(0.0);
    }

    glm::dvec3 minBounds(std::numeric_limits<double>::max());
    glm::dvec3 maxBounds(std::numeric_limits<double>::lowest());

    for (size_t i = 0; i < soA.position.size(); i++)
    {
        if (soA.type[i] == ParticleType::STRUCTURE)
        {
            const glm::dvec3 &pos = soA.position[i];
            minBounds = glm::min(minBounds, pos);
            maxBounds = glm::max(maxBounds, pos);
        }
    }

    glm::dvec3 structureSize(0.0);
    if (minBounds.x <= maxBounds.x) // Check if we actually found any STRUCTURE particles
    {
        structureSize = maxBounds - minBounds;
    }
    double cubeSize = structureSize.x * 0.5;

    glm::dvec3 cubePos = center + glm::dvec3(0.0, -cubeSize - 0.1, 0.0);

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

    for (int i = 0; i < numBalls; i++)
    {
        glm::dvec3 pos = startPos + glm::dvec3(i * spacing, 0.0, 0.0);
        soA.position.push_back(pos);
        soA.velocity.push_back(glm::dvec3(0.0));
        soA.forceAccum.push_back(glm::dvec3(0.0));
        soA.mass.push_back(10.0);
        soA.type.push_back(ParticleType::STRUCTURE);

        bool sflag = (bothEndsStatic) ? (i == 0 || i == (numBalls - 1)) : (i == 0);
        soA.isStatic.push_back(sflag);

        soA.color.push_back(glm::dvec3(1.0, 0.0, 0.0));
        soA.dimensions.push_back(glm::dvec3(0.1));
        soA.clothID.push_back(clothID);
    }

    // Create springs
    for (int i = 0; i < numBalls - 1; i++)
    {
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
    soA.dimensions.resize(startIndex + numParticles, glm::dvec3(0.03));
    soA.clothID.resize(startIndex + numParticles, clothID);

    double halfWidth = (gridSize - 1) * cellSize / 2.0;
    for (int i = 0; i < gridSize; i++)
    {
        for (int j = 0; j < gridSize; j++)
        {
            int localIndex = i * gridSize + j;
            size_t idx = startIndex + localIndex;
            soA.position[idx] =
                glm::dvec3(j * cellSize - halfWidth, 0.0, i * cellSize - halfWidth);
        }
    }

    // Left & right edges static
    for (int i = 0; i < gridSize; i++)
    {
        size_t leftIdx = startIndex + i * gridSize;
        size_t rightIdx = startIndex + i * gridSize + (gridSize - 1);
        soA.isStatic[leftIdx] = true;
        soA.isStatic[rightIdx] = true;
    }

    // Springs horizontally, vertically, diagonally
    for (int i = 0; i < gridSize; i++)
    {
        for (int j = 0; j < gridSize; j++)
        {
            size_t idx = startIndex + (i * gridSize + j);

            // Right neighbor
            if (j < gridSize - 1)
            {
                size_t rightIdx = startIndex + (i * gridSize + (j + 1));
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
            if (i < gridSize - 1)
            {
                size_t bottomIdx = startIndex + ((i + 1) * gridSize + j);
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
            if (i < gridSize - 1 && j < gridSize - 1)
            {
                size_t diagIdx = startIndex + ((i + 1) * gridSize + (j + 1));
                double dist = glm::distance(soA.position[idx], soA.position[diagIdx]);
                SpringData sp;
                sp.p1Index = (int)idx;
                sp.p2Index = (int)diagIdx;
                sp.restLength = dist * springRestLength;
                sp.springConstant = sharedParams.springConstant;
                sp.damping = 0.5;
                springs.push_back(sp);
            }
            if (i < gridSize - 1 && j > 0)
            {
                size_t diagIdx = startIndex + ((i + 1) * gridSize + (j - 1));
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
    int totalParticles = nLayers * particlesPerLayer;

    soA.position.resize(startIndex + totalParticles);
    soA.velocity.resize(startIndex + totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(startIndex + totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(startIndex + totalParticles, massValue);
    soA.type.resize(startIndex + totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(startIndex + totalParticles, false);
    soA.color.resize(startIndex + totalParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(startIndex + totalParticles, glm::dvec3(0.03));
    soA.clothID.resize(startIndex + totalParticles, clothID);

    double halfWidth = (gridSize - 1) * cellSize / 2.0;

    // Position each layer
    for (int l = 0; l < nLayers; l++)
    {
        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                int localIndex = l * particlesPerLayer + i * gridSize + j;
                size_t idx = startIndex + localIndex;

                double x = j * cellSize - halfWidth;
                double z = i * cellSize - halfWidth;
                double y = l * layerSpacing;
                soA.position[idx] = glm::dvec3(x, y, z);
            }
        }
    }

    // Left & right edges static in each layer
    for (int l = 0; l < nLayers; l++)
    {
        for (int i = 0; i < gridSize; i++)
        {
            size_t leftIdx = startIndex + (l * particlesPerLayer) + (i * gridSize);
            size_t rightIdx = startIndex + (l * particlesPerLayer) + (i * gridSize + (gridSize - 1));
            soA.isStatic[leftIdx] = true;
            soA.isStatic[rightIdx] = true;
        }
    }

    // Springs within each layer
    for (int l = 0; l < nLayers; l++)
    {
        int layerOffset = startIndex + (l * particlesPerLayer);

        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                int localIndex = i * gridSize + j;
                size_t idx = layerOffset + localIndex;

                // Right neighbor
                if (j < gridSize - 1)
                {
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
                if (i < gridSize - 1)
                {
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
                // Diagonal
                if (i < gridSize - 1 && j < gridSize - 1)
                {
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
                if (i < gridSize - 1 && j > 0)
                {
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

    // Springs between layers
    for (int l = 0; l < nLayers - 1; l++)
    {
        int lowerOffset = startIndex + (l * particlesPerLayer);
        int upperOffset = startIndex + ((l + 1) * particlesPerLayer);

        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                size_t lowerIdx = lowerOffset + (i * gridSize + j);
                for (int di = -1; di <= 1; di++)
                {
                    for (int dj = -1; dj <= 1; dj++)
                    {
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni < 0 || ni >= gridSize)
                            continue;
                        if (nj < 0 || nj >= gridSize)
                            continue;

                        size_t upperIdx = upperOffset + (ni * gridSize + nj);
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
    std::set<std::pair<int, int>> edgeSet;
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
    soA.dimensions.resize(oldCount + newCount, glm::dvec3(0.03));
    soA.clothID.resize(oldCount + newCount, clothID);

    double minX = 1e9, maxX = -1e9;
    for (auto &p : uniquePositions)
    {
        if (p.x < minX)
            minX = p.x;
        if (p.x > maxX)
            maxX = p.x;
    }

    for (size_t i = 0; i < newCount; i++)
    {
        size_t idx = oldCount + i;
        soA.position[idx] = uniquePositions[i];
    }
    for (size_t i = 0; i < newCount; i++)
    {
        size_t idx = oldCount + i;
        double px = soA.position[idx].x;
        bool leftStatic = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[idx] = (leftStatic || rightStatic);
    }

    // Now create springs from edgeSet
    for (auto &e : edgeSet)
    {
        int i1 = e.first + (int)oldCount;
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

void SimulationBase::createMultiLayerHexGrid(int numHexagons,
                                             double hexagonSize,
                                             double springRestLength,
                                             bool bothEndsStatic,
                                             double orientationDegrees,
                                             double layerHeight,
                                             int nLayers)
{
    // If you want to completely reset the scene’s springs (but NOT positions),
    // keep this. Otherwise, remove it if you want to accumulate.
    springs.clear();

    hexagonVertexLists.clear();
    hexagonIndices.clear();
    hexTriangles.clear();

    // -- Create a rotation matrix from your orientation degrees --
    glm::dmat4 rotationMatrix = createRotationMatrix(orientationDegrees);

    // -- Generate base (single-layer) hex pattern --
    std::vector<glm::dvec3> baseUniquePositions;
    std::set<std::pair<int, int>> edgeSet;
    generateHexagonCells(numHexagons, hexagonSize, rotationMatrix,
                         baseUniquePositions, edgeSet);

    const size_t baseCount = baseUniquePositions.size();
    if (baseCount == 0)
        return; // Safety check.

    // -- We will append to the SoA. Figure out where we start. --
    size_t oldCount = soA.position.size();
    size_t totalParticles = static_cast<size_t>(nLayers) * baseCount;

    // Resize the SoA arrays to hold new particles
    soA.position.resize(oldCount + totalParticles);
    soA.velocity.resize(oldCount + totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(oldCount + totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 5.0;
    soA.mass.resize(oldCount + totalParticles, massValue);
    soA.type.resize(oldCount + totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(oldCount + totalParticles, false);
    soA.color.resize(oldCount + totalParticles, glm::dvec3(1.0));
    soA.dimensions.resize(oldCount + totalParticles, glm::dvec3(0.03));

    // -------------------------------------------------------
    // 1) Identify an axis we’ll treat as “vertical”
    //    and two in-plane axes
    // -------------------------------------------------------
    // Compute bounding box of the single-layer geometry
    glm::dvec3 minPos = baseUniquePositions[0];
    glm::dvec3 maxPos = baseUniquePositions[0];
    for (size_t i = 1; i < baseCount; i++)
    {
        minPos = glm::min(minPos, baseUniquePositions[i]);
        maxPos = glm::max(maxPos, baseUniquePositions[i]);
    }
    glm::dvec3 diffs = maxPos - minPos;

    // Find whichever axis in (x,y,z) is smallest => call that “verticalAxis”
    glm::dvec3 verticalAxis(0.0);
    if (diffs.x <= diffs.y && diffs.x <= diffs.z)
    {
        verticalAxis = glm::dvec3(1.0, 0.0, 0.0);
    }
    else if (diffs.y <= diffs.x && diffs.y <= diffs.z)
    {
        verticalAxis = glm::dvec3(0.0, 1.0, 0.0);
    }
    else
    {
        verticalAxis = glm::dvec3(0.0, 0.0, 1.0);
    }

    // Create two perpendicular in-plane axes
    glm::dvec3 arbitrary;
    if (fabs(verticalAxis.z) < 0.9)
        arbitrary = glm::dvec3(0.0, 0.0, 1.0);
    else
        arbitrary = glm::dvec3(0.0, 1.0, 0.0);

    glm::dvec3 inPlaneAxis1 = glm::normalize(glm::cross(verticalAxis, arbitrary));
    glm::dvec3 inPlaneAxis2 = glm::normalize(glm::cross(verticalAxis, inPlaneAxis1));

    // -------------------------------------------------------
    // 2) Lay out all layers
    // -------------------------------------------------------
    for (int layer = 0; layer < nLayers; layer++)
    {
        for (size_t i = 0; i < baseCount; i++)
        {
            // global index in the SoA
            size_t idx = oldCount + layer * baseCount + i;

            glm::dvec3 pos = baseUniquePositions[i];
            // Move upward by (layerHeight * layer)
            pos += verticalAxis * (layerHeight * layer);

            // Optionally offset odd layers slightly in-plane (like the second snippet does)
            if (layer % 2 == 1)
            {
                pos += inPlaneAxis2 * hexagonSize;
            }

            soA.position[idx] = pos;
        }
    }

    // -------------------------------------------------------
    // 3) Assign red/green colors in a hex-based pattern
    //    (matching the second snippet)
    // -------------------------------------------------------
    if (!hexagonVertexLists.empty())
    {
        // For each layer, walk each hex’s vertex list
        for (int layer = 0; layer < nLayers; layer++)
        {
            for (const auto &hex : hexagonVertexLists)
            {
                // Find center of this hex
                glm::dvec3 center(0.0);
                for (auto &v : hex)
                    center += v;
                center /= (double)hex.size();

                // Sort each hex’s vertices by angle around the center
                std::vector<std::pair<double, glm::dvec3>> angleVerts;
                angleVerts.reserve(hex.size());
                for (auto &v : hex)
                {
                    double a = atan2(v.y - center.y, v.x - center.x);
                    angleVerts.push_back({a, v});
                }
                std::sort(angleVerts.begin(), angleVerts.end(),
                          [](auto &A, auto &B)
                          { return A.first > B.first; });

                // Assign alternating red/green
                for (size_t localIdx = 0; localIdx < angleVerts.size(); localIdx++)
                {
                    const glm::dvec3 &vertex = angleVerts[localIdx].second;

                    // Find which baseUniquePositions index this vertex corresponds to
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, vertex);
                    if (baseIndex < 0)
                        continue;

                    size_t globalIdx = oldCount + layer * baseCount + baseIndex;
                    if (localIdx % 2 == 0)
                        soA.color[globalIdx] = glm::dvec3(1.0, 0.0, 0.0); // red
                    else
                        soA.color[globalIdx] = glm::dvec3(0.0, 1.0, 0.0); // green
                }
            }
        }
    }
    else
    {
        std::cout << "No hexagonVertexLists found; assigning alternating red/green globally.\n";
        // If we have no hex polygons stored, just alternate red/green globally
        for (size_t i = 0; i < totalParticles; i++)
        {
            size_t globalIdx = oldCount + i;
            if (i % 2 == 0)
                soA.color[globalIdx] = glm::dvec3(1.0, 0.0, 0.0);
            else
                soA.color[globalIdx] = glm::dvec3(0.0, 1.0, 0.0);
        }
    }

    // --- Set Static Particles Along the Left and (optionally) Right Edges ---
    glm::dvec3 globalX(1.0, 0.0, 0.0);
    glm::dvec3 horizontalAxis = globalX - (glm::dot(globalX, verticalAxis) * verticalAxis);
    double eps = 0.001;
    if (glm::length(horizontalAxis) < eps)
    {
        horizontalAxis = glm::dvec3(0.0, 1.0, 0.0) -
                         (glm::dot(glm::dvec3(0.0, 1.0, 0.0), verticalAxis) * verticalAxis);
    }
    horizontalAxis = glm::normalize(horizontalAxis);
    double edgeTolerance = 0.05;

    for (int layer = 0; layer < nLayers; layer++)
    {
        size_t layerStart = layer * baseCount;
        size_t layerEnd = layerStart + baseCount;
        double minProj = 1e9, maxProj = -1e9;
        for (size_t i = layerStart; i < layerEnd; i++)
        {
            glm::dvec3 pp = soA.position[i];
            double proj = glm::dot(pp, horizontalAxis);
            if (proj < minProj)
                minProj = proj;
            if (proj > maxProj)
                maxProj = proj;
        }
        for (size_t i = layerStart; i < layerEnd; i++)
        {
            glm::dvec3 pp = soA.position[i];
            double proj = glm::dot(pp, horizontalAxis);
            if (fabs(proj - minProj) < edgeTolerance)
            {
                soA.isStatic[i] = true;
            }
            if (bothEndsStatic && fabs(proj - maxProj) < edgeTolerance)
            {
                soA.isStatic[i] = true;
            }
        }
    }

    // --- Intra-layer Springs ---
    for (int layer = 0; layer < nLayers; layer++)
    {
        for (const auto &edge : edgeSet)
        {
            int i1 = layer * baseCount + edge.first;
            int i2 = layer * baseCount + edge.second;
            double dist = glm::distance(soA.position[i1], soA.position[i2]);
            SpringData sp;
            sp.p1Index = i1;
            sp.p2Index = i2;
            sp.restLength = dist * sharedParams.springRestLength;
            sp.springConstant = sharedParams.springConstant;
            sp.damping = sharedParams.dampingCoefficient;
            springs.push_back(sp);
        }
    }

    // --- Inter-layer Springs (Using Float Precision) ---
    if (!hexagonVertexLists.empty())
    {
        for (int layer = 0; layer < nLayers - 1; layer++)
        {
            for (const auto &hex : hexagonVertexLists)
            {
                std::vector<size_t> lowerGreenIndices;
                std::vector<glm::vec3> lowerGreenPositions;
                std::vector<size_t> upperRedIndices;
                std::vector<glm::vec3> upperRedPositions;

                for (const glm::dvec3 &baseVertex : hex)
                {
                    int baseIndex = findApproxVertexIndex(baseUniquePositions, baseVertex);
                    if (baseIndex == -1)
                        continue;
                    size_t lowerGlobalIndex = layer * baseCount + baseIndex;
                    size_t upperGlobalIndex = (layer + 1) * baseCount + baseIndex;

                    // Convert color to float and compare as in Code 1.
                    if (approxEqualVec3(glm::vec3(soA.color[lowerGlobalIndex]),
                                        glm::vec3(1.0f, 0.0f, 0.0f)))
                    {
                        lowerGreenIndices.push_back(lowerGlobalIndex);
                        lowerGreenPositions.push_back(glm::vec3(soA.position[lowerGlobalIndex]));
                    }
                    if (approxEqualVec3(glm::vec3(soA.color[upperGlobalIndex]),
                                        glm::vec3(0.0f, 1.0f, 0.0f)))
                    {
                        upperRedIndices.push_back(upperGlobalIndex);
                        upperRedPositions.push_back(glm::vec3(soA.position[upperGlobalIndex]));
                    }
                }

                // --- Bottom-up pass ---
                if (!lowerGreenPositions.empty() && !upperRedPositions.empty())
                {
                    glm::vec3 avgLowerGreen(0.0f);
                    for (const auto &p : lowerGreenPositions)
                        avgLowerGreen += p;
                    avgLowerGreen /= static_cast<float>(lowerGreenPositions.size());

                    size_t chosenUpperRed = 0;
                    float bestDistance = std::numeric_limits<float>::max();
                    for (size_t i = 0; i < upperRedPositions.size(); i++)
                    {
                        float d = glm::distance(upperRedPositions[i], avgLowerGreen);
                        if (d < bestDistance)
                        {
                            bestDistance = d;
                            chosenUpperRed = upperRedIndices[i];
                        }
                    }

                    for (size_t i = 0; i < lowerGreenIndices.size(); i++)
                    {
                        SpringData sp;
                        sp.p1Index = lowerGreenIndices[i];
                        sp.p2Index = chosenUpperRed;
                        float dist = glm::distance(glm::vec3(soA.position[sp.p1Index]),
                                                   glm::vec3(soA.position[sp.p2Index]));
                        sp.restLength = sharedParams.springRestLength * dist;
                        sp.springConstant = sharedParams.springConstant;
                        sp.damping = sharedParams.dampingCoefficient;
                        springs.push_back(sp);
                    }
                }

                // --- Top-down pass ---
                if (!lowerGreenPositions.empty() && !upperRedPositions.empty())
                {
                    glm::vec3 avgUpperRed(0.0f);
                    for (const auto &p : upperRedPositions)
                        avgUpperRed += p;
                    avgUpperRed /= static_cast<float>(upperRedPositions.size());

                    size_t chosenLowerGreen = 0;
                    float bestDistance = std::numeric_limits<float>::max();
                    for (size_t i = 0; i < lowerGreenPositions.size(); i++)
                    {
                        float d = glm::distance(lowerGreenPositions[i], avgUpperRed);
                        if (d < bestDistance)
                        {
                            bestDistance = d;
                            chosenLowerGreen = lowerGreenIndices[i];
                        }
                    }

                    for (size_t i = 0; i < upperRedIndices.size(); i++)
                    {
                        SpringData sp;
                        sp.p1Index = chosenLowerGreen;
                        sp.p2Index = upperRedIndices[i];
                        float dist = glm::distance(glm::vec3(soA.position[sp.p1Index]),
                                                   glm::vec3(soA.position[sp.p2Index]));
                        sp.restLength = sharedParams.springRestLength * dist;
                        sp.springConstant = sharedParams.springConstant;
                        sp.damping = sharedParams.dampingCoefficient;
                        springs.push_back(sp);
                    }
                }
            }
        }
    }
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
                                          const glm::dmat4 &rotationMatrix,
                                          std::vector<glm::dvec3> &uniquePositions,
                                          std::set<std::pair<int, int>> &edgeSet)
{
    auto findOrAdd = [&](const glm::dvec3 &pos) -> int
    {
        double eps = 0.0001;
        for (int i = 0; i < (int)uniquePositions.size(); i++)
        {
            glm::dvec3 diff = uniquePositions[i] - pos;
            if (glm::length(diff) < eps)
            {
                return i;
            }
        }
        uniquePositions.push_back(pos);
        return (int)uniquePositions.size() - 1;
    };

    // Simple hex layout, now also storing each hexagon’s vertices.
    for (int r = 0; r < numHexagons; r++)
    {
        for (int c = 0; c < numHexagons; c++)
        {
            glm::dvec3 center;
            center.x = std::sqrt(3.0) * hexagonSize * (c + (r % 2) * 0.5);
            center.y = 1.5 * hexagonSize * r;
            center.z = 0.0;
            center = glm::dvec3(rotationMatrix * glm::dvec4(center, 1.0));

            // Temporary containers for this hexagon’s vertices and indices.
            std::vector<glm::dvec3> currVerts;
            std::vector<int> indices;
            currVerts.reserve(6);
            indices.reserve(6);

            for (int i = 0; i < 6; i++)
            {
                double ang = glm::radians(60.0 * i + 90.0);
                glm::dvec3 offset(
                    hexagonSize * std::cos(ang),
                    hexagonSize * std::sin(ang),
                    0.0);
                offset = glm::dvec3(rotationMatrix * glm::dvec4(offset, 0.0));
                glm::dvec3 vertex = center + offset;
                // Save vertex in the current hexagon container.
                currVerts.push_back(vertex);
                indices.push_back(findOrAdd(vertex));
            }
            // Save this hexagon's vertices and indices.
            hexagonVertexLists.push_back(currVerts);
            hexagonIndices.push_back(indices);

            // Build edges from the indices.
            for (int i = 0; i < 6; i++)
            {
                int i1 = indices[i];
                int i2 = indices[(i + 1) % 6];
                if (i1 > i2)
                    std::swap(i1, i2);
                edgeSet.insert({i1, i2});
            }
        }
    }
}

// ------------------ Legacy functions -------------- //
void SimulationBase::assignUniquePositionsToSoA(const std::vector<glm::dvec3> &uniquePositions,
                                                bool bothEndsStatic)
{
    size_t n = uniquePositions.size();
    soA.position.resize(n);
    soA.velocity.resize(n, glm::dvec3(0.0));
    soA.forceAccum.resize(n, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(n, massValue);
    soA.type.resize(n, ParticleType::STRUCTURE);
    soA.isStatic.resize(n, false);
    soA.color.resize(n, glm::dvec3(1.0, 0.4, 0.0));
    soA.dimensions.resize(n, glm::dvec3(0.03));

    double minX = 1e9, maxX = -1e9;
    for (auto &p : uniquePositions)
    {
        if (p.x < minX)
            minX = p.x;
        if (p.x > maxX)
            maxX = p.x;
    }
    for (size_t i = 0; i < n; i++)
    {
        soA.position[i] = uniquePositions[i];
    }
    for (size_t i = 0; i < n; i++)
    {
        double px = soA.position[i].x;
        bool leftStatic = (px <= minX + 0.001);
        bool rightStatic = (bothEndsStatic && px >= maxX - 0.001);
        soA.isStatic[i] = (leftStatic || rightStatic);
    }
}

void SimulationBase::createSpringsFromEdgeSet(const std::set<std::pair<int, int>> &edgeSet,
                                              double springRestLength)
{
    for (auto &e : edgeSet)
    {
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

bool SimulationBase::approxEqualVec3(const glm::dvec3 &a, const glm::dvec3 &b, double epsilon)
{
    return (fabs(a.x - b.x) < epsilon) &&
           (fabs(a.y - b.y) < epsilon) &&
           (fabs(a.z - b.z) < epsilon);
}

int SimulationBase::findApproxVertexIndex(const std::vector<glm::dvec3> &vertices, const glm::dvec3 &target, double epsilon)
{
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        if (approxEqualVec3(vertices[i], target, epsilon))
        {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found.
}

void SimulationBase::createMultiLayerSquareGridWithDiagonalsCentered(
    int cx, int cy, int cz,  // Grid coordinate of the block center
    int gridSize,            // Number of particles per row/column in each layer
    int nLayers,             // Number of layers in the cloth (vertical stack)
    double cellSize,         // Spacing between cloth particles in a layer
    double layerSpacing,     // Spacing between layers
    double springRestLength) // Factor to scale the computed rest lengths of springs
{
    // ----------------------------
    // 1) Create Particles & Springs
    // ----------------------------
    size_t startIndex = soA.position.size();
    int clothID = nextClothID++; // Assign a unique cloth ID

    int particlesPerLayer = gridSize * gridSize;
    int totalParticles = nLayers * particlesPerLayer;

    soA.position.resize(startIndex + totalParticles);
    soA.velocity.resize(startIndex + totalParticles, glm::dvec3(0.0));
    soA.forceAccum.resize(startIndex + totalParticles, glm::dvec3(0.0));
    double massValue = guiInstance ? guiInstance->getParticleMass() : 1.0;
    soA.mass.resize(startIndex + totalParticles, massValue);
    soA.type.resize(startIndex + totalParticles, ParticleType::STRUCTURE);
    soA.isStatic.resize(startIndex + totalParticles, false); // all dynamic
    soA.color.resize(startIndex + totalParticles, glm::dvec3(1.0, 0.0, 0.0));
    soA.dimensions.resize(startIndex + totalParticles, glm::dvec3(0.03));
    soA.clothID.resize(startIndex + totalParticles, clothID);

    glm::dvec3 blockCenter = glm::dvec3(cx, cy, cz);
    double halfWidth = (gridSize - 1) * cellSize / 2.0;

    // Position particles for each layer (grid in xz, y offset per layer)
    for (int l = 0; l < nLayers; l++)
    {
        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                int localIndex = l * particlesPerLayer + i * gridSize + j;
                size_t idx = startIndex + localIndex;
                double x = j * cellSize - halfWidth;
                double z = i * cellSize - halfWidth;
                double y = l * layerSpacing;
                soA.position[idx] = blockCenter + glm::dvec3(x, y, z);
            }
        }
    }

    // Create springs within each layer (horizontal, vertical, and diagonal)
    for (int l = 0; l < nLayers; l++)
    {
        int layerOffset = startIndex + (l * particlesPerLayer);
        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                int localIndex = i * gridSize + j;
                size_t idx = layerOffset + localIndex;
                if (j < gridSize - 1)
                {
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
                if (i < gridSize - 1)
                {
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
                if (i < gridSize - 1 && j < gridSize - 1)
                {
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
                if (i < gridSize - 1 && j > 0)
                {
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

    // Create springs between layers
    for (int l = 0; l < nLayers - 1; l++)
    {
        int lowerOffset = startIndex + (l * particlesPerLayer);
        int upperOffset = startIndex + ((l + 1) * particlesPerLayer);
        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                size_t lowerIdx = lowerOffset + (i * gridSize + j);
                for (int di = -1; di <= 1; di++)
                {
                    for (int dj = -1; dj <= 1; dj++)
                    {
                        int ni = i + di, nj = j + dj;
                        if (ni < 0 || ni >= gridSize || nj < 0 || nj >= gridSize)
                            continue;
                        size_t upperIdx = upperOffset + (ni * gridSize + nj);
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

    // ----------------------------
    // 2) Generate Outer Shell Mesh for the Cloth
    //    (Top face, Bottom face, and Side Faces)
    // ----------------------------

    // Clear previous triangle data.
    hexTriangles.clear();

    // Helper lambda: computes face normal and pushes a HexTriangle.
    auto addHexTriangle = [&](const glm::vec3 &p0, const glm::vec3 &p1, const glm::vec3 &p2)
    {
        HexTriangle tri;
        tri.vertices[0] = p0;
        tri.vertices[1] = p1;
        tri.vertices[2] = p2;
        tri.normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        hexTriangles.push_back(tri);
    };

    auto getPos = [&](int layer, int i, int j) -> glm::vec3
    {
        size_t index = startIndex + (layer * gridSize * gridSize) + (i * gridSize) + j;
        return glm::vec3(soA.position[index]);
    };

    // --- Top Face (upper layer) ---
    {
        int topLayer = nLayers - 1;
        for (int i = 0; i < gridSize - 1; i++)
        {
            for (int j = 0; j < gridSize - 1; j++)
            {
                glm::vec3 p00 = getPos(topLayer, i, j);
                glm::vec3 p01 = getPos(topLayer, i, j + 1);
                glm::vec3 p10 = getPos(topLayer, i + 1, j);
                glm::vec3 p11 = getPos(topLayer, i + 1, j + 1);
                // Create two triangles per cell.
                addHexTriangle(p00, p01, p10);
                addHexTriangle(p01, p11, p10);
            }
        }
    }

    // --- Bottom Face (lower layer; reverse winding so normal points downward) ---
    {
        int bottomLayer = 0;
        for (int i = 0; i < gridSize - 1; i++)
        {
            for (int j = 0; j < gridSize - 1; j++)
            {
                glm::vec3 p00 = getPos(bottomLayer, i, j);
                glm::vec3 p01 = getPos(bottomLayer, i, j + 1);
                glm::vec3 p10 = getPos(bottomLayer, i + 1, j);
                glm::vec3 p11 = getPos(bottomLayer, i + 1, j + 1);
                // Reverse the order for downward normals.
                addHexTriangle(p00, p10, p01);
                addHexTriangle(p01, p10, p11);
            }
        }
    }

    // --- Side Faces: Connect the perimeter between consecutive layers ---
    for (int l = 0; l < nLayers - 1; l++)
    {
        // Helper lambda for adding a quad (split into 2 triangles)
        auto addQuad = [&](const glm::vec3 &A, const glm::vec3 &B,
                           const glm::vec3 &C, const glm::vec3 &D)
        {
            addHexTriangle(A, B, C);
            addHexTriangle(B, D, C);
        };

        // Front edge (i = 0)
        {
            int i = 0;
            for (int j = 0; j < gridSize - 1; j++)
            {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i, j + 1);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i, j + 1);
                addQuad(A, B, C, D);
            }
        }
        // Back edge (i = gridSize - 1); reverse winding for outward normals.
        {
            int i = gridSize - 1;
            for (int j = 0; j < gridSize - 1; j++)
            {
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
            for (int i = 0; i < gridSize - 1; i++)
            {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i + 1, j);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i + 1, j);
                addQuad(A, B, C, D);
            }
        }
        // Right edge (j = gridSize - 1); reverse winding for outward normals.
        {
            int j = gridSize - 1;
            for (int i = 0; i < gridSize - 1; i++)
            {
                glm::vec3 A = getPos(l, i, j);
                glm::vec3 B = getPos(l, i + 1, j);
                glm::vec3 C = getPos(l + 1, i, j);
                glm::vec3 D = getPos(l + 1, i + 1, j);
                addQuad(A, C, B, D);
            }
        }
    }

    // Compute total vertex count (6 floats per vertex).
    int vertexCount = static_cast<int>(meshData.size() / 6);

    // ----------------------------
    // 3) Create VAO/VBO for This Cloth Mesh and Store It
    // ----------------------------
    // Build the initial mesh data.
    std::vector<float> meshData = buildClothMeshData(startIndex, gridSize, nLayers);

    // Create VAO/VBO, but use GL_DYNAMIC_DRAW so we can update it later.
    unsigned int meshVAO, meshVBO;
    glGenVertexArrays(1, &meshVAO);
    glGenBuffers(1, &meshVBO);
    glBindVertexArray(meshVAO);
    glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(float), meshData.data(), GL_DYNAMIC_DRAW); // DYNAMIC now!

    // Set up vertex attributes.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Create the ClothMesh object.
    ClothMesh clothMesh;
    clothMesh.vao = meshVAO;
    clothMesh.vbo = meshVBO;
    clothMesh.vertexCount = static_cast<int>(meshData.size() / 6);
    clothMesh.startIndex = startIndex;
    clothMesh.gridSize = gridSize;
    clothMesh.nLayers = nLayers;
    clothMeshes.push_back(clothMesh);
}