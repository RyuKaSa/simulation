#include "Renderer.hpp"

// Maximum supported instances.
const size_t Renderer::maxInstances;

Renderer::Renderer() {
    // Load shaders from the shaders/ folder.
    if (!ballShader.load("src/shaders/ball.vs.glsl", "src/shaders/ball.fs.glsl")) {
        std::cerr << "Failed to load ball shaders." << std::endl;
    }
    if (!cubeShader.load("src/shaders/cube.vs.glsl", "src/shaders/cube.fs.glsl")) {
        std::cerr << "Failed to load cube shaders." << std::endl;
    }
    
    // Initialize each sub-component.
    initBallGeometry();
    initGrid();
    initCube();
    initSprings();
    initHexTriangles();
    
    targetDistance = 10.0f;
    cameraPosition = glm::vec3(0.0f, 0.0f, targetDistance);
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
}

Renderer::~Renderer() {
    glDeleteProgram(ballShader.getID());
    glDeleteProgram(cubeShader.getID());

    // Delete ball geometry.
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &instancePosVBO);
    glDeleteBuffers(1, &instanceColorVBO);
    glDeleteBuffers(1, &instanceScaleVBO);

    // Delete grid.
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    // Delete springs.
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);

    // Delete hexagon triangles.
    glDeleteVertexArrays(1, &hexTriVAO);
    glDeleteBuffers(1, &hexTriVBO);

    // Delete cube geometry.
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
}

void Renderer::initBallGeometry() {
    float radius = 0.1f;
    float vertices[(numSegments + 2) * 3];
    // The center vertex:
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
    // Create a circle using a triangle fan.
    for (int i = 0; i <= numSegments; i++) {
        float angle = i * 2.0f * 3.1415926f / numSegments;
        vertices[(i + 1) * 3 + 0] = radius * cos(angle);
        vertices[(i + 1) * 3 + 1] = radius * sin(angle);
        vertices[(i + 1) * 3 + 2] = 0.0f;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create instance buffers for position, color, and scale.
    glGenBuffers(1, &instancePosVBO);
    glGenBuffers(1, &instanceColorVBO);
    glGenBuffers(1, &instanceScaleVBO);

    // Instance attribute: offset (location 1)
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // Instance attribute: instColor (location 2)
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // Instance attribute: scale (location 3)
    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void Renderer::initCube() {
    // Define the cube vertices and indices (a unit cube centered at the origin).
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f, -0.5f,  
         0.5f,  0.5f, -0.5f,  
        -0.5f,  0.5f, -0.5f,  
        -0.5f, -0.5f,  0.5f,  
         0.5f, -0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f   
    };
    unsigned int cubeIndices[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void Renderer::initSprings() {
    glGenVertexArrays(1, &springVAO);
    glGenBuffers(1, &springVBO);
}

void Renderer::initHexTriangles() {
    glGenVertexArrays(1, &hexTriVAO);
    glGenBuffers(1, &hexTriVBO);
    glBindVertexArray(hexTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::initGrid() {
    std::vector<float> gridVertices;
    float gridSize = 300.0f;
    float spacing = 5.0f;
    
    for (float x = -gridSize; x <= gridSize; x += spacing) {
        gridVertices.push_back(x); gridVertices.push_back(-gridSize); gridVertices.push_back(0.0f);
        gridVertices.push_back(x); gridVertices.push_back(gridSize);  gridVertices.push_back(0.0f);
    }
    for (float y = -gridSize; y <= gridSize; y += spacing) {
        gridVertices.push_back(-gridSize); gridVertices.push_back(y); gridVertices.push_back(0.0f);
        gridVertices.push_back(gridSize);  gridVertices.push_back(y); gridVertices.push_back(0.0f);
    }
    
    gridVertexCount = gridVertices.size() / 3;
    
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::render(const Simulation& simulation) {
    adjustCameraToFit(simulation);
    
    cameraPosition = glm::mix(cameraPosition, targetPosition, lerpFactor);
    cameraTarget   = glm::mix(cameraTarget, targetCenter, lerpFactor);
    
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Build projection and view matrices.
    auto projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 1000.0f);
    auto view       = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Render the scene using the ball shader.
    ballShader.use();
    ballShader.setUniform("uModel", glm::mat4(1.0f));
    ballShader.setUniform("uMVP", projection * view);
    
    renderGrid(projection, view);
    renderSprings(simulation, projection, view);
    renderHexTriangles(simulation, projection, view);
    renderBalls(simulation, projection, view);
    
    // Render external cubes with the separate cube shader.
    renderExternalCubes(simulation, projection, view);
}

void Renderer::renderExternalCubes(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    const ParticleSoA& soa = simulation.getSoA();
    std::vector<glm::vec3> positions, scales, colors;
    for (size_t i = 0; i < soa.position.size(); i++) {
        if (soa.type[i] == ParticleType::EXTERNAL) {
            positions.push_back(soa.position[i]);
            scales.push_back(soa.dimensions[i]);
            colors.push_back(soa.color[i]);
        }
    }
    if (positions.empty())
        return;
    
    // Save polygon mode and line width.
    GLint prevPolygonMode;
    glGetIntegerv(GL_POLYGON_MODE, &prevPolygonMode);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GLfloat prevLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    glLineWidth(5.0f);
    
    // Use the cube shader.
    cubeShader.use();
    for (size_t i = 0; i < positions.size(); i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, positions[i]);
        float s = scales[i].x;
        model = glm::scale(model, glm::vec3(s));
        glm::mat4 mvp = projection * view * model;
        cubeShader.setUniform("uMVP", mvp);
        cubeShader.setUniform("uColor", colors[i]);
        
        // Bind the pre-built cube geometry.
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    glPolygonMode(GL_FRONT_AND_BACK, prevPolygonMode);
    glLineWidth(prevLineWidth);
    
    // Restore ball shader for subsequent draws.
    ballShader.use();
}

void Renderer::renderBalls(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    const ParticleSoA& soa = simulation.getSoA();
    std::vector<glm::vec3> positions, colors, scales;
    for (size_t i = 0; i < soa.position.size(); i++) {
        if (soa.type[i] != ParticleType::EXTERNAL) {
            positions.push_back(soa.position[i]);
            colors.push_back(soa.color[i]);
            scales.push_back(soa.dimensions[i]);
        }
    }
    size_t instanceCount = positions.size();
    if (instanceCount > maxInstances)
        instanceCount = maxInstances;
    
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    void* posPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (posPtr) {
        memcpy(posPtr, positions.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), positions.data());
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    void* colorPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (colorPtr) {
        memcpy(colorPtr, colors.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), colors.data());
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    void* scalePtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (scalePtr) {
        memcpy(scalePtr, scales.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), scales.data());
    }
    
    ballShader.setUniform("useInstance", 1);
    glm::mat4 model = glm::mat4(1.0f);
    ballShader.setUniform("uModel", model);
    glm::mat4 mvp = projection * view * model;
    ballShader.setUniform("uMVP", mvp);
    
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, static_cast<GLsizei>(instanceCount));
    glBindVertexArray(0);
}

void Renderer::renderSprings(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    const ParticleSoA& soa = simulation.getSoA();
    const std::vector<SpringData>& springs = simulation.getSprings();
    std::vector<glm::vec3> springVertices;
    springVertices.reserve(springs.size() * 2);
    for (const SpringData& spring : springs) {
        springVertices.push_back(soa.position[spring.p1Index]);
        springVertices.push_back(soa.position[spring.p2Index]);
    }
    ballShader.setUniform("useInstance", 0);
    ballShader.setUniform("uColor", glm::vec3(0.25f, 0.39f, 0.59f));
    glm::mat4 model = glm::mat4(1.0f);
    ballShader.setUniform("uModel", model);
    glm::mat4 mvp = projection * view * model;
    ballShader.setUniform("uMVP", mvp);
    
    glBindVertexArray(springVAO);
    glBindBuffer(GL_ARRAY_BUFFER, springVBO);
    glBufferData(GL_ARRAY_BUFFER, springVertices.size() * sizeof(glm::vec3),
                 springVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(springVertices.size()));
    glBindVertexArray(0);
}

void Renderer::renderHexTriangles(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    const std::vector<HexTriangle>& tris = simulation.getHexTriangles();
    if (tris.empty())
        return;
    
    const size_t vertexCount = tris.size() * 3;
    const size_t totalBytes = vertexCount * sizeof(glm::vec3);
    
    glBindVertexArray(hexTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);
    glBufferData(GL_ARRAY_BUFFER, totalBytes, nullptr, GL_DYNAMIC_DRAW);
    
    glm::vec3* bufferData = static_cast<glm::vec3*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
    if (bufferData) {
        for (const auto& tri : tris) {
            memcpy(bufferData, tri.vertices, 3 * sizeof(glm::vec3));
            bufferData += 3;
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        std::vector<glm::vec3> triVertices;
        triVertices.reserve(vertexCount);
        for (const auto& tri : tris) {
            for (int i = 0; i < 3; i++) {
                triVertices.push_back(tri.vertices[i]);
            }
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, triVertices.size() * sizeof(glm::vec3), triVertices.data());
    }
    
    ballShader.setUniform("useInstance", 0);
    ballShader.setUniform("uColor", glm::vec3(0.42f, 0.65f, 0.99f));
    glm::mat4 modelMat = glm::mat4(1.0f);
    ballShader.setUniform("uModel", modelMat);
    glm::mat4 mvpMat = projection * view * modelMat;
    ballShader.setUniform("uMVP", mvpMat);
    
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
    glBindVertexArray(0);
}

void Renderer::renderGrid(const glm::mat4& projection, const glm::mat4& view) {
    // Build model and MVP matrices.
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;
    
    // Set the uniforms via the ballShader.
    ballShader.use();
    ballShader.setUniform("uModel", model);
    ballShader.setUniform("uMVP", mvp);
    
    // Bind the grid VAO and draw the grid lines.
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void Renderer::adjustCameraToFit(const Simulation& simulation) {
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty())
        return;
    
    const size_t n = positions.size();
    glm::vec3 minPos = positions[0];
    glm::vec3 maxPos = positions[0];
    for (size_t i = 0; i < n; i += 3) {
        minPos = glm::min(minPos, positions[i]);
        maxPos = glm::max(maxPos, positions[i]);
    }
    if ((n - 1) % 3 != 0) {
        minPos = glm::min(minPos, positions.back());
        maxPos = glm::max(maxPos, positions.back());
    }
    glm::vec3 center = (minPos + maxPos) * 0.5f;
    float maxExtent = glm::length(maxPos - minPos);
    float newDistance = glm::clamp(maxExtent * 0.8f, minCameraDistance, maxCameraDistance);
    targetDistance = glm::mix(targetDistance, newDistance, lerpFactor);
    targetCenter = center;
    targetPosition = glm::vec3(maxPos.x, maxPos.y, center.z + targetDistance);
}

void Renderer::cameraReset(const Simulation& simulation) {
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty())
        return;
    glm::vec3 minPos = positions[0];
    glm::vec3 maxPos = positions[0];
    glm::vec3 sum(0.0f);
    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
        sum += pos;
    }
    glm::vec3 center = sum / static_cast<float>(positions.size());
    float extent = glm::length(maxPos - minPos);
    extent = glm::min(extent, maxCameraDistance);
    float desiredDistance = glm::max(extent * 1.1f, minCameraDistance);
    if (glm::length(center) > maxCameraDistance) {
        center = glm::normalize(center) * maxCameraDistance;
    }
    cameraTarget = center;
    targetCenter = center;
    targetDistance = desiredDistance;
    targetPosition = center;
    cameraPosition = targetPosition;
}