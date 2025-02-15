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

uniform mat4 uMVP;
uniform mat4 uModel;
uniform bool useInstance;   // If true, use instance data
uniform vec3 uColor;        // Fallback color when not instanced

out vec3 fColor;

void main()
{
    vec4 pos;
    if(useInstance) {
        pos = vec4(aPos + offset, 1.0);
        fColor = instColor;
    } else {
        pos = uModel * vec4(aPos, 1.0);
        fColor = uColor;
    }
    gl_Position = uMVP * pos;
}
)glsl";

// Simplified Fragment Shader using the interpolated color
const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 fColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(fColor, 1.0);
}
)glsl";

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
    glDeleteBuffers(1, &instanceVBO);
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);
    glDeleteVertexArrays(1, &hexVAO);
    glDeleteBuffers(1, &hexVBO);
}

void Renderer::init() {
    initShaders();
    
    // Create ball geometry (a circle)
    float radius = 0.1f;
    float vertices[(numSegments + 2) * 3];
    // Center vertex
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
    for (int i = 0; i <= numSegments; i++) {
        float angle = i * 2.0f * 3.1415926f / numSegments;
        vertices[(i + 1) * 3 + 0] = radius * cos(angle);
        vertices[(i + 1) * 3 + 1] = radius * sin(angle);
        vertices[(i + 1) * 3 + 2] = 0.0f;
    }
    
    // Setup ball VAO and VBO
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // aPos attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Create instance VBO for per-instance offset and color (each instance: 2 vec3's)
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    // Initially no data; will update each frame.
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    // Instance offset attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    // Instance color attribute (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    glBindVertexArray(0);
    
    initGrid();
    
    // Initialize spring VAO/VBO (for batched spring rendering)
    glGenVertexArrays(1, &springVAO);
    glGenBuffers(1, &springVBO);

    // Initialize hexagon VAO/VBO (for hex grid rendering)
    glGenVertexArrays(1, &hexVAO);
    glGenBuffers(1, &hexVBO);
}

void Renderer::initGrid() {
    // Create grid lines on the X-Y plane covering a large region.
    std::vector<float> gridVertices;
    float gridSize = 300.0f;
    float spacing = 4.0f;
    
    // Vertical lines
    for (float x = -gridSize; x <= gridSize; x += spacing) {
        gridVertices.push_back(x); gridVertices.push_back(-gridSize); gridVertices.push_back(0.0f);
        gridVertices.push_back(x); gridVertices.push_back(gridSize);  gridVertices.push_back(0.0f);
    }
    // Horizontal lines
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
    // Use cached uniform locations instead of calling glGetUniformLocation
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);  // Grid color
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
    
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex Shader Compilation Failed: " << infoLog << std::endl;
    }
    
    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment Shader Compilation Failed: " << infoLog << std::endl;
    }
    
    // Link shaders into program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader Program Linking Failed: " << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Cache uniform locations for later use
    mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    useInstanceLoc = glGetUniformLocation(shaderProgram, "useInstance");
    gridSpacingLoc = glGetUniformLocation(shaderProgram, "gridSpacing");
}

void Renderer::render(const Simulation& simulation) {
    adjustCameraToFit(simulation);
    
    // Smooth camera transition (lerp)
    cameraPosition = glm::mix(cameraPosition, targetPosition, lerpFactor);
    cameraTarget = glm::mix(cameraTarget, targetCenter, lerpFactor);
    
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    
    auto projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 1000.0f);
    auto view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Render grid (disable instancing)
    glUniform1i(useInstanceLoc, 0);
    {
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        renderGrid(projection, view);
    }

    // Render hex hitboxes
    renderHexHitboxes(simulation, projection, view);
    
    // --- Render Springs (batched) ---
    std::vector<glm::vec3> springEndpoints = simulation.getSpringEndpoints();
    if (!springEndpoints.empty()) {
        // Compute a small offset in the direction from the target to the camera
        glm::vec3 camDir = glm::normalize(cameraPosition - cameraTarget);
        glm::vec3 offset = 0.01f * camDir; // springs moved 0.001 closer to the camera

        // Apply the offset to each spring endpoint (only for rendering)
        for (auto &pt : springEndpoints) {
            pt += offset;
        }
        
        glBindVertexArray(springVAO);
        glBindBuffer(GL_ARRAY_BUFFER, springVBO);
        glBufferData(GL_ARRAY_BUFFER, springEndpoints.size() * sizeof(glm::vec3),
                    springEndpoints.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // For springs, disable instancing.
        glUniform1i(useInstanceLoc, 0);
        glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f); // Blue springs
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        glDrawArrays(GL_LINES, 0, springEndpoints.size());
        glBindVertexArray(0);
    }
    
    // --- Render Balls with Instanced Rendering ---
    const auto& balls = simulation.getBalls();
    std::vector<glm::vec3> velocities = simulation.getParticleVelocities();

    // 1) Determine maximum speed among *structure only*:
    float maxStructureSpeed = 0.01f; // a small nonzero to avoid divide-by-zero
    for (size_t i = 0; i < balls.size(); i++)
    {
        if (balls[i].type == ParticleType::STRUCTURE) {
            float speed = glm::length(velocities[i]);
            if (speed > maxStructureSpeed) {
                maxStructureSpeed = speed;
            }
        }
    }

    // 2) Build instance data for rendering:
    std::vector<float> instanceData;
    instanceData.reserve(balls.size() * 6);  // 3 for position + 3 for color

    for (size_t i = 0; i < balls.size(); i++)
    {
        const auto& ball = balls[i];
        glm::vec3 color;

        // if ParticleType::EXTERNAL
        if (ball.type == ParticleType::EXTERNAL) {
            color = glm::vec3(0.0f, 1.0f, 0.0f); // green
        }
        else {
            // STRUCTURE: color from green to red based on speed
            float speed = glm::length(velocities[i]);
            float normalized = (maxStructureSpeed < 1e-6f)
                            ? 0.0f
                            : glm::clamp(speed / maxStructureSpeed, 0.0f, 1.0f);
            color = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f),  // green
                            glm::vec3(1.0f, 0.0f, 0.0f),  // red
                            normalized);
        }

        // Position
        instanceData.push_back(ball.position.x);
        instanceData.push_back(ball.position.y);
        instanceData.push_back(ball.position.z);
        // Color
        instanceData.push_back(color.r);
        instanceData.push_back(color.g);
        instanceData.push_back(color.b);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float),
                instanceData.data(), GL_DYNAMIC_DRAW);

    // Enable instancing for ball rendering.
    glUniform1i(useInstanceLoc, 1);
    {
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    }
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, balls.size());
    glBindVertexArray(0);
}

void Renderer::renderHexHitboxes(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view) {
    // Get all hitbox triangle vertices from the simulation.
    std::vector<glm::vec3> triangles = simulation.getHexHitboxTriangles();
    if (triangles.empty())
        return;
    
    // Disable instancing (we're doing simple batched drawing)
    glUniform1i(useInstanceLoc, 0);
    // Set a light blue color (e.g., RGB: 0.68, 0.85, 0.90)
    glUniform3f(colorLoc, 0.68f, 0.85f, 0.90f);
    
    // Set model and MVP matrices (here, we assume an identity model)
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    
    // Bind our hex hitbox VAO and update its vertex buffer with the batched triangle data.
    glBindVertexArray(hexVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexVBO);
    glBufferData(GL_ARRAY_BUFFER, triangles.size() * sizeof(glm::vec3), triangles.data(), GL_DYNAMIC_DRAW);
    // Attribute 0: vertex position (3 floats per vertex)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Draw all triangles in one call.
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangles.size()));
    
    glBindVertexArray(0);
}

void Renderer::adjustCameraToFit(const Simulation& simulation) {
    // Use only structure particle positions.
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

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
    float newDistance = maxExtent * 1.1f;  // Add some padding

    const float minDistance = 5.0f;
    const float maxDistance = 200.0f;
    newDistance = glm::clamp(newDistance, minDistance, maxDistance);

    targetDistance = glm::mix(targetDistance, newDistance, lerpFactor);
    targetCenter = center;
    targetPosition = center + glm::vec3(0.0f, 0.0f, targetDistance);
}

void Renderer::cameraReset(const Simulation& simulation) {
    // Retrieve structure particle positions
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

    // Compute bounding box and center
    glm::vec3 minPos = positions[0];
    glm::vec3 maxPos = positions[0];
    glm::vec3 sum(0.0f);
    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
        sum += pos;
    }
    glm::vec3 center = sum / static_cast<float>(positions.size());

    // Compute extent and clamp it to avoid extreme values
    float extent = glm::length(maxPos - minPos);
    const float maxAllowedExtent = 200.0f; // maximum extent considered reasonable
    extent = glm::min(extent, maxAllowedExtent);

    // Determine the new camera distance with a minimum fallback
    const float minDistance = 5.0f;
    float desiredDistance = glm::max(extent * 1.1f, minDistance);

    const float maxCenterDistance = 200.0f;
    if (glm::length(center) > maxCenterDistance) {
        center = glm::normalize(center) * maxCenterDistance;
    }

    // Immediately update camera variables (bypassing the lerp)
    cameraTarget = center;
    targetCenter = center;
    targetDistance = desiredDistance;
    targetPosition = center + glm::vec3(0.0f, 0.0f, 0.0f);
    cameraPosition = targetPosition; // directly set the camera position
}