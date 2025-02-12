#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

// Simple vertex and fragment shaders for flat colored balls.
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 FragPos;  // Send world position to fragment shader

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0)); // Compute world position
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 FragPos;  
out vec4 FragColor;
uniform vec3 uColor;
uniform float gridSpacing; // Grid line distance

void main() {
    // Compute grid lines using mod function
    float gridX = mod(abs(FragPos.x), gridSpacing) < 0.05 ? 1.0 : 0.0;
    float gridY = mod(abs(FragPos.y), gridSpacing) < 0.05 ? 1.0 : 0.0;

    float gridIntensity = max(gridX, gridY); // Combine horizontal and vertical grid lines

    // Blend grid color with object color (grid lines are gray)
    vec3 gridColor = mix(uColor, vec3(0.7, 0.7, 0.7), gridIntensity * 0.5);

    FragColor = vec4(gridColor, 1.0);
}
)glsl";

Renderer::Renderer() {
    init();
    cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f);
    cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
}

Renderer::~Renderer() {
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer::init() {
    initShaders();
    float radius = 0.1f;
    float vertices[(numSegments+2) * 3];
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f; // center
    for (int i = 0; i <= numSegments; i++) {
        float angle = i * 2.0f * 3.1415926f / numSegments;
        vertices[(i+1)*3 + 0] = radius * cos(angle);
        vertices[(i+1)*3 + 1] = radius * sin(angle);
        vertices[(i+1)*3 + 2] = 0.0f;
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    initGrid();
}

void Renderer::initGrid() {
    // We'll create grid lines on the X-Y plane, covering a region from -20 to 20 in both X and Y.
    std::vector<float> gridVertices;
    float gridSize = 50.0f;
    float spacing = 1.0f;  // You can adjust spacing as desired
    
    // Vertical lines: constant X, Y goes from -gridSize to gridSize
    for (float x = -gridSize; x <= gridSize; x += spacing) {
        gridVertices.push_back(x); gridVertices.push_back(-gridSize); gridVertices.push_back(0.0f);
        gridVertices.push_back(x); gridVertices.push_back(gridSize);  gridVertices.push_back(0.0f);
    }
    // Horizontal lines: constant Y, X goes from -gridSize to gridSize
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
}

void Renderer::renderGrid(const glm::mat4& projection, const glm::mat4& view) {

    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);  // dark gray grid
    
    unsigned int gridLoc = glGetUniformLocation(shaderProgram, "gridSpacing");
    glUniform1f(gridLoc, 1.0f);

    glm::mat4 model = glm::mat4(1.0f);
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    glm::mat4 mvp = projection * view * model;
    unsigned int mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
}

void Renderer::initShaders() {
    int success;
    char infoLog[512];
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex Shader Compilation Failed: " << infoLog << std::endl;
    }
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment Shader Compilation Failed: " << infoLog << std::endl;
    }
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
}

void Renderer::render(const Simulation& simulation) {
    adjustCameraToFit(simulation);

    // Apply Lerp to Camera
    cameraPosition = glm::mix(cameraPosition, targetPosition, lerpFactor);
    cameraTarget = glm::mix(cameraTarget, targetCenter, lerpFactor);

    // Clear the screen
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);

    // Perspective camera using lerped values
    auto projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 100.0f);
    auto view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    // First, render the grid
    renderGrid(projection, view);

    // Retrieve velocities from Simulation for the balls
    std::vector<glm::vec3> velocities = simulation.getParticleVelocities();

    // Find the maximum velocity magnitude
    float maxSpeed = 0.01f;
    for (const auto& velocity : velocities) {
        float speed = glm::length(velocity);
        if (speed > maxSpeed) maxSpeed = speed;
    }

    // Render each ball with velocity-based color
    for (size_t i = 0; i < simulation.getBalls().size(); i++) {
        float speed = glm::length(velocities[i]);
        float normalizedSpeed = glm::clamp(speed / maxSpeed, 0.0f, 1.0f);

        glm::vec3 color = glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), // Green (slow)
                                   glm::vec3(1.0f, 0.0f, 0.0f), // Red (fast)
                                   normalizedSpeed);

        const Ball& ball = simulation.getBalls()[i];

        glm::mat4 model = glm::translate(glm::mat4(1.0f), ball.position);
        glm::mat4 mvp = projection * view * model;

        unsigned int mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform3f(colorLoc, color.r, color.g, color.b);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, numSegments+2);
    }
}

void Renderer::adjustCameraToFit(const Simulation& simulation) {
    std::vector<glm::vec3> positions = simulation.getParticlePositions();
    
    if (positions.empty()) return;

    // Compute bounding box
    glm::vec3 minPos = positions[0];
    glm::vec3 maxPos = positions[0];
    glm::vec3 center(0.0f);

    for (const auto& pos : positions) {
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos);
        center += pos;
    }
    center /= static_cast<float>(positions.size());

    // Compute camera distance based on bounding box size
    float maxExtent = glm::length(maxPos - minPos);
    float distance = maxExtent * 2.0f;  // Increase for padding

    // Set target values for Lerp
    targetPosition = glm::vec3(center.x, center.y, distance);
    targetCenter   = center;  // Look at the center of the system
}