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
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)glsl";

Renderer::Renderer() {
    init();
}

Renderer::~Renderer() {
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer::init() {
    initShaders();
    // Use class member numSegments
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
    // Clear the screen to white.
    glClearColor(1.0f,1.0f,1.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    // Use an orthographic projection for simplicity.
    glm::mat4 projection = glm::ortho(-1.0f,1.0f,-1.0f,1.0f,-1.0f,1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    // Render each ball.
    for (const auto& ball : simulation.getBalls()) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), ball.position);
        glm::mat4 mvp = projection * view * model;
        unsigned int mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glUniform3f(colorLoc, ball.color.r, ball.color.g, ball.color.b);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, numSegments+2);
    }
}