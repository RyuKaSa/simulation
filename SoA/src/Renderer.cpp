#include <glad/glad.h>
#include <SDL_opengl.h>
#include "Renderer.hpp"

// Maximum supported instances
const size_t Renderer::maxInstances;

Renderer::Renderer(const SimulationBase& simulation) {
    if (!ballShader.load("src/shaders/ball.vs.glsl", "src/shaders/ball.fs.glsl")) {
        std::cerr << "Failed to load ball shaders." << std::endl;
    }
    if (!cubeShader.load("src/shaders/cube.vs.glsl", "src/shaders/cube.fs.glsl")) {
        std::cerr << "Failed to load cube shaders." << std::endl;
    }
    if (!springShader.load("src/shaders/spring.vs.glsl", "src/shaders/spring.fs.glsl")) {
        std::cerr << "Failed to load spring shaders." << std::endl;
    }
    if (!meshShader.load("src/shaders/mesh.vs.glsl", "src/shaders/mesh.fs.glsl")) {
        std::cerr << "Failed to load mesh shaders." << std::endl;
    }
    if (!gridShader.load("src/shaders/grid.vs.glsl", "src/shaders/grid.fs.glsl")) {
        std::cerr << "Failed to load grid shaders." << std::endl;
    }

    initBallGeometry();
    // initGrid();
    initCube();
    initSprings();
    // initHexTriangles();
    initTripleGrid(simulation);

    targetDistance = 10.0f;
    cameraPosition = glm::vec3(0.0f, 0.0f, targetDistance);
    cameraTarget   = glm::vec3(0.0f, 0.0f, 0.0f);
}

Renderer::~Renderer() {
    glDeleteProgram(ballShader.getID());
    glDeleteProgram(cubeShader.getID());
    glDeleteProgram(springShader.getID());
    glDeleteProgram(meshShader.getID());
    glDeleteProgram(gridShader.getID());
    

    // Delete ball geometry
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &instancePosVBO);
    glDeleteBuffers(1, &instanceColorVBO);
    glDeleteBuffers(1, &instanceScaleVBO);

    // Delete grid
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    // Delete springs
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);

    // Delete hex triangles
    glDeleteVertexArrays(1, &hexTriVAO);
    glDeleteBuffers(1, &hexTriVBO);

    // Delete cube geometry
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
}

void Renderer::initBallGeometry() {
    float radius = 0.1f;
    float vertices[(numSegments + 2) * 3];
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &instancePosVBO);
    glGenBuffers(1, &instanceColorVBO);
    glGenBuffers(1, &instanceScaleVBO);

    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances*sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances*sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances*sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void Renderer::initCube() {
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::initGrid() {
    std::vector<float> gridVertices;
    float gridSize = 300.0f;
    float spacing = 5.0f;

    for (float x = -gridSize; x <= gridSize; x += spacing) {
        gridVertices.push_back(x);
        gridVertices.push_back(-gridSize);
        gridVertices.push_back(0.0f);

        gridVertices.push_back(x);
        gridVertices.push_back(gridSize);
        gridVertices.push_back(0.0f);
    }
    for (float y = -gridSize; y <= gridSize; y += spacing) {
        gridVertices.push_back(-gridSize);
        gridVertices.push_back(y);
        gridVertices.push_back(0.0f);

        gridVertices.push_back(gridSize);
        gridVertices.push_back(y);
        gridVertices.push_back(0.0f);
    }

    gridVertexCount = (int)(gridVertices.size() / 3);

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 gridVertices.size()*sizeof(float),
                 gridVertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::render(const SimulationBase& simulation) {
    adjustCameraToFit(simulation);

    cameraPosition = glm::mix(cameraPosition, targetPosition, lerpFactor);
    cameraTarget   = glm::mix(cameraTarget,   targetCenter,  lerpFactor);

    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    auto projection = glm::perspective(
        glm::radians(45.0f),
        16.0f/9.0f,
        0.1f, 1000.0f
    );
    auto view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f,1.0f,0.0f));

    ballShader.use();
    ballShader.setUniform("uModel", glm::mat4(1.0f));
    ballShader.setUniform("uMVP", projection * view);

    renderGrid(projection, view);
    renderSprings(simulation, projection, view);
    // renderHexTriangles(simulation, projection, view);
    // renderBalls(simulation, projection, view);

    // std::cout << "Rendering complete 1" << std::endl;
    renderExternalCubes(simulation, projection, view);
    // std::cout << "Rendering complete 2" << std::endl;
}

void Renderer::renderExternalCubes(const SimulationBase& simulation,
                                   const glm::mat4& projection,
                                   const glm::mat4& view)
{
    // std::cout << "Rendering external cubes START" << std::endl;
    
    // We'll do the minimal double->float conversion
    const ParticleSoA soa = simulation.getSoACopy();
    // std::cout << "after simulation.getSoA()" << std::endl;
    std::vector<glm::vec3> positions, scales, colors;
    // std::cout << "after vector init" << std::endl;
    positions.reserve(soa.position.size());
    scales.reserve(soa.dimensions.size());
    colors.reserve(soa.color.size());
    // std::cout << "after reserve" << std::endl;

    for (size_t i = 0; i < soa.position.size(); i++) {
        if (soa.type[i] == ParticleType::EXTERNAL) {
            // Convert dvec3 -> vec3
            glm::vec3 p(
                (float)soa.position[i].x,
                (float)soa.position[i].y,
                (float)soa.position[i].z
            );
            glm::vec3 s(
                (float)soa.dimensions[i].x,
                (float)soa.dimensions[i].y,
                (float)soa.dimensions[i].z
            );
            glm::vec3 c(
                (float)soa.color[i].x,
                (float)soa.color[i].y,
                (float)soa.color[i].z
            );
            positions.push_back(p);
            scales.push_back(s);
            colors.push_back(c);
        }
    }
    // this redundant loop fixes a memory issue ???
    for (size_t i = 0; i < soa.position.size(); i++) {
        if (soa.type[i] == ParticleType::EXTERNAL) {
            glm::vec3 c(
                (float)soa.color[i].x,
                (float)soa.color[i].y,
                (float)soa.color[i].z
            );
            colors.push_back(c);
        }
    }

    // std::cout << "colors.size() AFTER filling = " << colors.size() << std::endl;
    volatile size_t dummy = colors.size();
    // std::cout << "after for loop" << std::endl;
    if (positions.empty()) return;
    // std::cout << "after if" << std::endl;

    GLint prevPolygonMode;
    glGetIntegerv(GL_POLYGON_MODE, &prevPolygonMode);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GLfloat prevLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);
    glLineWidth(5.0f);
    // std::cout << "before cubeShader.use()" << std::endl;

    cubeShader.use();
    // std::cout << "after cubeShader.use()" << std::endl;
    // std::cout << "cubeShader is valid? " << &cubeShader << std::endl;

    for (size_t i = 0; i < positions.size(); i++) {
        // std::cout << "in for loop" << std::endl;
        glm::mat4 model(1.0f);
        model = glm::translate(model, positions[i]);
        float s = scales[i].x;
        if (std::isnan(s) || std::isinf(s) || s == 0.0f) {
            std::cerr << "ERROR: Invalid scale value: " << s << std::endl;
            return;
        }
        model = glm::scale(model, glm::vec3(s));
        // std::cout << "after model" << std::endl;

        // std::cout << "scales.size() = " << scales.size() << ", i = " << i << std::endl;
        if (i >= scales.size()) {
            std::cerr << "ERROR: Index out of bounds in scales!" << std::endl;
            return;
        }

        glm::mat4 mvp = projection * view * model;
        // std::cout << "after mvp" << std::endl;
        cubeShader.setUniform("uMVP", mvp);
        // std::cout << "after setUniform mvp" << std::endl;
        // std::cout << "colors.size() = " << colors.size() << std::endl;
        if (colors.empty()) {
            std::cerr << "ERROR: colors vector is empty!" << std::endl;
            return;
        }
        // std::cout << "Color: " << colors[i].r << ", " << colors[i].g << ", " << colors[i].b << std::endl;
        if (std::isnan(colors[i].r) || std::isnan(colors[i].g) || std::isnan(colors[i].b) ||
            std::isinf(colors[i].r) || std::isinf(colors[i].g) || std::isinf(colors[i].b)) {
            std::cerr << "ERROR: Invalid color values!" << std::endl;
            return;
        }
        cubeShader.setUniform("uColor", colors[i]);
        // std::cout << "after setUniform color" << std::endl;

        glBindVertexArray(cubeVAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    // std::cout << "after for loop" << std::endl;

    glPolygonMode(GL_FRONT_AND_BACK, prevPolygonMode);
    glLineWidth(prevLineWidth);

    ballShader.use();
    // std::cout << "Rendering external cubes END" << std::endl;
}

void Renderer::initTripleGrid(const SimulationBase& simulation)
{
    // 1) Compute bounding box by skipping every 3 positions
    const ParticleSoA &soa = simulation.getSoA();
    if (soa.position.empty()) {
        // If no particles, just do nothing
        gridVertexCount = 0;
        std::cerr << "No particles to create grid for!" << std::endl;
        return;
    }

    glm::dvec3 minPos(DBL_MAX), maxPos(-DBL_MAX);
    // We'll also ensure we read the first and last elements
    // so that we definitely include the extremes
    minPos = glm::min(minPos, soa.position.front());
    maxPos = glm::max(maxPos, soa.position.front());
    minPos = glm::min(minPos, soa.position.back());
    maxPos = glm::max(maxPos, soa.position.back());

    // skip ~ every 3rd to quickly get bounding box
    for (size_t i = 0; i < soa.position.size(); i += 3) {
        minPos = glm::min(minPos, soa.position[i]);
        maxPos = glm::max(maxPos, soa.position[i]);
    }

    // Expand the bounding box a bit so the planes are "just outside"
    double margin = 0.1 * glm::length(maxPos - minPos);  // 10% margin
    if (margin < 0.5) margin = 0.5; // minimal margin
    glm::dvec3 expand(margin, margin, margin);
    minPos -= expand;
    maxPos += expand;

    // For clarity, convert to floats now
    float minX = (float)minPos.x;
    float minY = (float)minPos.y;
    float minZ = (float)minPos.z;
    float maxX = (float)maxPos.x;
    float maxY = (float)maxPos.y;
    float maxZ = (float)maxPos.z;

    // 2) Decide the plane positions
    // We'll do XY-plane at z = minZ
    // XZ-plane at y = minY
    // YZ-plane at x = minX

    // We'll define a spacing. You can choose how dense you want it
    float spacing = 2.0f;  // or pick your own step
    // (You can also base spacing on the bounding box size if you want.)

    std::vector<float> gridVertices;
    gridVertices.reserve(100000); // Just a guess

    // Helper lambda to build line pairs
    auto addLine = [&](float x1, float y1, float z1,
                       float x2, float y2, float z2)
    {
        gridVertices.push_back(x1);
        gridVertices.push_back(y1);
        gridVertices.push_back(z1);

        gridVertices.push_back(x2);
        gridVertices.push_back(y2);
        gridVertices.push_back(z2);
    };

    // ============ PLANE 1: XY-plane at z = minZ ============
    // We'll draw lines parallel to X and parallel to Y
    for (float x = minX; x <= maxX; x += spacing) {
        addLine(x, minY, minZ, x, maxY, minZ);
    }
    for (float y = minY; y <= maxY; y += spacing) {
        addLine(minX, y, minZ, maxX, y, minZ);
    }

    // ============ PLANE 2: XZ-plane at y = minY ============
    for (float x = minX; x <= maxX; x += spacing) {
        addLine(x, minY, minZ, x, minY, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += spacing) {
        addLine(minX, minY, z, maxX, minY, z);
    }

    // ============ PLANE 3: YZ-plane at x = minX ============
    for (float y = minY; y <= maxY; y += spacing) {
        addLine(minX, y, minZ, minX, y, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += spacing) {
        addLine(minX, minY, z, minX, maxY, z);
    }

    // Now we know how many vertices we have
    gridVertexCount = (int)(gridVertices.size() / 3);
    std::cout << "Grid vertex count: " << gridVertexCount << std::endl;

    // 3) Create and upload to the VBO/VAO
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 gridVertices.size() * sizeof(float),
                 gridVertices.data(),
                 GL_STATIC_DRAW);

    // Simple position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::renderBalls(const SimulationBase& simulation,
                           const glm::mat4& projection,
                           const glm::mat4& view)
{
    // --- Use the ball shader first! ---
    ballShader.use();
    
    const ParticleSoA& soa = simulation.getSoA();
    std::vector<glm::vec3> positions, colors, scales;
    positions.reserve(soa.position.size());
    colors.reserve(soa.color.size());
    scales.reserve(soa.dimensions.size());

    // Filter out EXTERNAL particles so we only see the 'balls'
    for (size_t i = 0; i < soa.position.size(); i++) {
        if (soa.type[i] != ParticleType::EXTERNAL) {
            positions.emplace_back((float)soa.position[i].x,
                                   (float)soa.position[i].y,
                                   (float)soa.position[i].z);
            colors.emplace_back((float)soa.color[i].x,
                                (float)soa.color[i].y,
                                (float)soa.color[i].z);
            scales.emplace_back((float)soa.dimensions[i].x,
                                (float)soa.dimensions[i].y,
                                (float)soa.dimensions[i].z);
        }
    }

    // Clamp instance count if needed
    size_t instanceCount = positions.size();
    if (instanceCount > maxInstances) {
        instanceCount = maxInstances;
    }

    // -- Upload instance data (positions, colors, scales) to the VBOs -- //
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    void* posPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                    instanceCount*sizeof(glm::vec3),
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (posPtr) {
        memcpy(posPtr, positions.data(), instanceCount*sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        // Fallback if mapping fails
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount*sizeof(glm::vec3),
                        positions.data());
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    void* colorPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                      instanceCount*sizeof(glm::vec3),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (colorPtr) {
        memcpy(colorPtr, colors.data(), instanceCount*sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount*sizeof(glm::vec3),
                        colors.data());
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    void* scalePtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                      instanceCount*sizeof(glm::vec3),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (scalePtr) {
        memcpy(scalePtr, scales.data(), instanceCount*sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount*sizeof(glm::vec3),
                        scales.data());
    }

    // Set uniforms
    ballShader.setUniform("useInstance", 1);
    
    glm::mat4 model(1.0f);
    ballShader.setUniform("uModel", model);
    glm::mat4 mvp = projection * view * model;
    ballShader.setUniform("uMVP", mvp);

    // Draw as instanced triangles
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, (GLsizei)instanceCount);
    glBindVertexArray(0);
}

void Renderer::renderSprings(const SimulationBase& simulation,
                             const glm::mat4& projection,
                             const glm::mat4& view)
{
    const ParticleSoA& soa = simulation.getSoA();
    const std::vector<SpringData>& springs = simulation.getSprings();

    std::vector<glm::vec3> springVertices;
    springVertices.reserve(springs.size()*2);

    for (const SpringData& spring : springs) {
        glm::vec3 p1 = glm::vec3(
            (float)soa.position[spring.p1Index].x,
            (float)soa.position[spring.p1Index].y,
            (float)soa.position[spring.p1Index].z
        );
        glm::vec3 p2 = glm::vec3(
            (float)soa.position[spring.p2Index].x,
            (float)soa.position[spring.p2Index].y,
            (float)soa.position[spring.p2Index].z
        );
        springVertices.push_back(p1);
        springVertices.push_back(p2);
    }

    springShader.use();
    glm::mat4 model(1.0f);
    glm::mat4 mvp = projection * view * model;
    springShader.setUniform("uMVP", mvp);
    springShader.setUniform("uColor", glm::vec3(0.25f, 0.39f, 0.59f));

    // Upload the vertex data
    glBindVertexArray(springVAO);
    glBindBuffer(GL_ARRAY_BUFFER, springVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 springVertices.size()*sizeof(glm::vec3),
                 springVertices.data(),
                 GL_DYNAMIC_DRAW);

    // Setup the vertex attrib again
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINES, 0, (GLsizei)springVertices.size());
    glBindVertexArray(0);
}

void Renderer::renderHexTriangles(const SimulationBase& simulation,
                                  const glm::mat4& projection,
                                  const glm::mat4& view)
{
    const std::vector<HexTriangle>& tris = simulation.getHexTriangles();
    if (tris.empty()) return;

    size_t vertexCount = tris.size() * 3;
    size_t totalBytes  = vertexCount*sizeof(glm::vec3);

    glBindVertexArray(hexTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);
    glBufferData(GL_ARRAY_BUFFER, totalBytes, nullptr, GL_DYNAMIC_DRAW);

    glm::vec3* bufferData = (glm::vec3*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (bufferData) {
        for (const auto& tri : tris) {
            memcpy(bufferData, tri.vertices, 3*sizeof(glm::vec3));
            bufferData += 3;
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    meshShader.use();
    glm::mat4 modelMat(1.0f);
    glm::mat4 mvpMat = projection * view * modelMat;
    meshShader.setUniform("uMVP", mvpMat);
    meshShader.setUniform("uColor", glm::vec3(0.42f, 0.65f, 0.99f));

    // Draw
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    glBindVertexArray(0);
}

void Renderer::renderGrid(const glm::mat4& projection, const glm::mat4& view)
{
    glm::mat4 model(1.0f);
    glm::mat4 mvp  = projection*view*model;

    gridShader.use();
    gridShader.setUniform("uModel", model);
    gridShader.setUniform("uMVP", mvp);
    gridShader.setUniform("uColor", glm::vec3(0.3f, 0.3f, 0.3f));

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void Renderer::adjustCameraToFit(const SimulationBase& simulation)
{
    std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

    size_t n = positions.size();
    glm::dvec3 minPos = positions[0];
    glm::dvec3 maxPos = positions[0];

    for (size_t i = 0; i < n; i += 3) {
        minPos = glm::min(minPos, positions[i]);
        maxPos = glm::max(maxPos, positions[i]);
    }
    if ((n - 1) % 3 != 0) {
        minPos = glm::min(minPos, positions.back());
        maxPos = glm::max(maxPos, positions.back());
    }
    glm::dvec3 center = (minPos + maxPos)*0.5;
    double maxExtent   = glm::length(maxPos - minPos);
    float newDistance  = (float)glm::clamp(maxExtent*0.8, (double)minCameraDistance, (double)maxCameraDistance);
    targetDistance     = glm::mix(targetDistance, newDistance, lerpFactor);

    targetCenter = glm::vec3((float)center.x, (float)center.y, (float)center.z);
    targetPosition = glm::vec3((float)maxPos.x,
                               (float)(maxPos.y + 0.3),
                               (float)(center.z + targetDistance));
}

void Renderer::cameraReset(const SimulationBase& simulation)
{
    std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

    glm::dvec3 minPos = positions[0];
    glm::dvec3 maxPos = positions[0];
    glm::dvec3 sum(0.0);

    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
        sum += pos;
    }
    glm::dvec3 center = sum / (double)positions.size();
    double extent = glm::length(maxPos - minPos);
    extent = glm::min(extent, (double)maxCameraDistance);
    float desiredDistance = (float)glm::max(extent*1.1, (double)minCameraDistance);
    if (glm::length(center) > (double)maxCameraDistance) {
        center = glm::normalize(center)*(double)maxCameraDistance;
    }
    cameraTarget = glm::vec3((float)center.x,
                             (float)center.y,
                             (float)center.z);
    targetCenter = cameraTarget;
    targetDistance = desiredDistance;
    targetPosition = cameraTarget;
    cameraPosition = targetPosition;
}