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
    glDeleteBuffers(1, &instanceVBO);
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);
}

void Renderer::init() {
    initShaders();
    
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
    
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    const GLsizeiptr instanceBufferSize = maxInstances * 7 * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, instanceBufferSize, nullptr, GL_DYNAMIC_DRAW);
    
    const GLsizei stride = 7 * sizeof(float);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    
    glBindVertexArray(0);
    
    initGrid();
    
    glGenVertexArrays(1, &springVAO);
    glGenBuffers(1, &springVBO);
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
    auto view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glUniform1i(useInstanceLoc, 0);
    {
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        renderGrid(projection, view);
    }
    
    // Render springs
    std::vector<glm::vec3> springEndpoints = simulation.getSpringEndpoints();
    if (!springEndpoints.empty()) {
        glm::vec3 camDir = glm::normalize(cameraPosition - cameraTarget);
        glm::vec3 offset = 0.01f * camDir;
        for (auto &pt : springEndpoints) {
            pt += offset;
        }
        
        glBindVertexArray(springVAO);
        glBindBuffer(GL_ARRAY_BUFFER, springVBO);
        glBufferData(GL_ARRAY_BUFFER, springEndpoints.size() * sizeof(glm::vec3),
                     springEndpoints.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glUniform1i(useInstanceLoc, 0);
        glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        glDrawArrays(GL_LINES, 0, springEndpoints.size());
        glBindVertexArray(0);
    }
    
    // Render Balls with instanced rendering
    BallSoA soa = simulation.getSnapshotSoA();
    size_t instanceCount = soa.posX.size();
    if (instanceCount > maxInstances)
        instanceCount = maxInstances;
    
    std::vector<glm::vec3> velocities = simulation.getParticleVelocities();
    float maxStructureSpeed = 0.01f;
    for (size_t i = 0; i < instanceCount; i++) {
        if (soa.types[i] == static_cast<int>(ParticleType::STRUCTURE)) {
            float speed = glm::length(velocities[i]);
            if (speed > maxStructureSpeed)
                maxStructureSpeed = speed;
        }
    }
    
    std::vector<float> instanceData;
    instanceData.reserve(instanceCount * 7);
    for (size_t i = 0; i < instanceCount; i++) {
        instanceData.push_back(soa.posX[i]);
        instanceData.push_back(soa.posY[i]);
        instanceData.push_back(soa.posZ[i]);
        instanceData.push_back(soa.colorR[i]);
        instanceData.push_back(soa.colorG[i]);
        instanceData.push_back(soa.colorB[i]);
        instanceData.push_back(soa.dimsX[i]);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float),
                 instanceData.data(), GL_DYNAMIC_DRAW);
    
    glUniform1i(useInstanceLoc, 1);
    {
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glm::mat4 mvp = projection * view * model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    }
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, instanceCount);
    glBindVertexArray(0);
}

void Renderer::adjustCameraToFit(const Simulation& simulation) {
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
    float newDistance = maxExtent * 1.1f;

    newDistance = glm::clamp(newDistance, minCameraDistance, maxCameraDistance);

    targetDistance = glm::mix(targetDistance, newDistance, lerpFactor);
    targetCenter = center;
    targetPosition = center + glm::vec3(0.0f, 0.0f, targetDistance);
}

void Renderer::cameraReset(const Simulation& simulation) {
    std::vector<glm::vec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

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