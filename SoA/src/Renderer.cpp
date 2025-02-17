#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>

// Vertex Shader with instancing support
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
// Instance attributes
layout (location = 1) in vec3 offset;   // Ball center
layout (location = 2) in vec3 instColor;  // Ball color
layout (location = 3) in float scale;     // Ball scale

uniform mat4 uMVP;
uniform mat4 uModel;
uniform int useInstance;
uniform vec3 uColor;

out vec3 fColor;

void main()
{
    vec4 posNonInstance = uModel * vec4(aPos, 1.0);
    vec4 posInstance = vec4(aPos * scale + offset, 1.0);
    vec4 pos = mix(posNonInstance, posInstance, float(useInstance));
    fColor = mix(uColor, instColor, float(useInstance));
    gl_Position = uMVP * pos;
}
)glsl";

// Fragment Shader
const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 fColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(fColor, 1.0);
}
)glsl";

const size_t Renderer::maxInstances;

Renderer::Renderer() {
    init();
    targetDistance = 10.0f;
    cameraPosition = glm::vec3(0.0f, 0.0f, targetDistance);
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
}

Renderer::~Renderer() {
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &instancePosVBO);
    glDeleteBuffers(1, &instanceColorVBO);
    glDeleteBuffers(1, &instanceScaleVBO);
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);
    glDeleteVertexArrays(1, &hexTriVAO);
    glDeleteBuffers(1, &hexTriVBO);
}

void Renderer::init() {
    initShaders();
    // Create ball geometry (a circle)
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Generate separate instance VBOs:
    glGenBuffers(1, &instancePosVBO);
    glGenBuffers(1, &instanceColorVBO);
    glGenBuffers(1, &instanceScaleVBO);
    // Attribute location 1: position (offset)
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    // Attribute location 2: color
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    // Attribute location 3: scale (using dimensions.x)
    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    initGrid();

    glGenVertexArrays(1, &springVAO);
    glGenBuffers(1, &springVBO);

    // Initialize hexagon triangles VAO/VBO (for batched rendering)
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
    float spacing = 4.0f;
    
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

void Renderer::renderGrid(const glm::mat4& projection, const glm::mat4& view) {
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);
    glUniform1f(gridSpacingLoc, 1.0f);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void Renderer::initShaders() {
    int success;
    char infoLog[512];
    
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex Shader Compilation Failed: " << infoLog << std::endl;
    }
    
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment Shader Compilation Failed: " << infoLog << std::endl;
    }
    
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader Program Linking Failed: " << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    useInstanceLoc = glGetUniformLocation(shaderProgram, "useInstance");
    gridSpacingLoc = glGetUniformLocation(shaderProgram, "gridSpacing");
}

void Renderer::render(const Simulation& simulation) {
    adjustCameraToFit(simulation);
    
    cameraPosition = glm::mix(cameraPosition, targetPosition, lerpFactor);
    cameraTarget   = glm::mix(cameraTarget, targetCenter, lerpFactor);
    
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    auto projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 1000.0f);
    auto view       = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    renderGrid(projection, view);
    // Render springs and hexagon triangles as before:
    renderSprings(simulation, projection, view);
    renderHexTriangles(simulation, projection, view);
    
    // Render Balls with instanced rendering using the SoA directly.
    renderBalls(simulation, projection, view);
}

void Renderer::renderBalls(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    // Retrieve particle data from the SoA.
    const ParticleSoA& soa = simulation.getSoA();
    size_t instanceCount = soa.position.size();
    if (instanceCount > maxInstances)
        instanceCount = maxInstances;

    // Update instance positions using buffer mapping for speed.
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    void* posPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (posPtr) {
        memcpy(posPtr, soa.position.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), soa.position.data());
    }

    // Update instance colors.
    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    void* colorPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (colorPtr) {
        memcpy(colorPtr, soa.color.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), soa.color.data());
    }

    // Update instance scales (dimensions).
    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    void* scalePtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3),
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (scalePtr) {
        memcpy(scalePtr, soa.dimensions.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceCount * sizeof(glm::vec3), soa.dimensions.data());
    }

    // Set shader state for instanced ball rendering.
    glUniform1i(useInstanceLoc, 1);
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    // Bind the ball VAO (created in init()) and draw all instances in one call.
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, static_cast<GLsizei>(instanceCount));
    glBindVertexArray(0);
}

void Renderer::renderSprings(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    // Retrieve the particle data from the simulation
    const ParticleSoA& soa = simulation.getSoA();
    
    // Retrieve the spring data.
    // (Assuming Simulation exposes a getter like getSprings(). If not, you could add one.)
    const std::vector<SpringData>& springs = simulation.getSprings();
    
    // Reserve space for two vertices per spring (one for each endpoint)
    std::vector<glm::vec3> springVertices;
    springVertices.reserve(springs.size() * 2);
    
    // For each spring, add both endpoints from the SoA.
    for (const SpringData& spring : springs) {
        // Using the spring's particle indices, get the positions
        springVertices.push_back(soa.position[spring.p1Index]);
        springVertices.push_back(soa.position[spring.p2Index]);
    }
    
    // Set up the shader state for non-instanced (simple line) drawing.
    glUniform1i(useInstanceLoc, 0);
    // Set the color to blue for springs.
    glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f);
    
    // Setup the model and MVP matrices.
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    
    // Bind the spring VAO and update its VBO with the batch of vertices.
    glBindVertexArray(springVAO);
    glBindBuffer(GL_ARRAY_BUFFER, springVBO);
    // Upload the spring vertex data in one batch.
    glBufferData(GL_ARRAY_BUFFER, springVertices.size() * sizeof(glm::vec3),
                 springVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Draw all spring lines in one call
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(springVertices.size()));
    
    // Unbind the VAO to clean up state.
    glBindVertexArray(0);
}

void Renderer::renderHexTriangles(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    // Retrieve the precomputed hexagon triangles from the simulation.
    const std::vector<HexTriangle>& tris = simulation.getHexTriangles();
    if (tris.empty())
        return;

    // Calculate total vertex count (3 vertices per triangle) and total size in bytes.
    const size_t vertexCount = tris.size() * 3;
    const size_t totalBytes = vertexCount * sizeof(glm::vec3);

    // Bind the VAO and allocate buffer memory (using DYNAMIC_DRAW for frequent updates).
    glBindVertexArray(hexTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);
    glBufferData(GL_ARRAY_BUFFER, totalBytes, nullptr, GL_DYNAMIC_DRAW);

    // Map the buffer and copy the triangle vertices in one batch.
    glm::vec3* bufferData = static_cast<glm::vec3*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
    if (bufferData) {
        for (const auto& tri : tris) {
            // Since the vertices are stored consecutively in the triangle, copy all three.
            memcpy(bufferData, tri.vertices, 3 * sizeof(glm::vec3));
            bufferData += 3;
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        // Fallback in case mapping fails (should be rare).
        std::vector<glm::vec3> triVertices;
        triVertices.reserve(vertexCount);
        for (const auto& tri : tris) {
            for (int i = 0; i < 3; i++) {
                triVertices.push_back(tri.vertices[i]);
            }
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, triVertices.size() * sizeof(glm::vec3), triVertices.data());
    }

    // Set shader state:
    // Disable instancing and set the triangle color to medium grey.
    glUniform1i(useInstanceLoc, 0);
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);

    // Set up the model and MVP matrices.
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    // Render all triangles in one batched call.
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));

    glBindVertexArray(0);
}

void Renderer::adjustCameraToFit(const Simulation& simulation) {
    // Use the new convenience getter for structure particle positions.
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty())
        return;

    glm::vec3 minPos = positions[0];
    glm::vec3 maxPos = positions[0];
    glm::vec3 center(0.0f);
    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
        center += pos;
    }
    center /= static_cast<float>(positions.size());

    float maxExtent = glm::length(maxPos - minPos);
    float newDistance = maxExtent * 1.1f;

    newDistance = glm::clamp(newDistance, minCameraDistance, maxCameraDistance);

    targetDistance = glm::mix(targetDistance, newDistance, lerpFactor);
    targetCenter = center;
    targetPosition = center + glm::vec3(0.0f, 0.0f, targetDistance);
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