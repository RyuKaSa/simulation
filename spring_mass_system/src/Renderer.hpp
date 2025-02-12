#ifndef RENDERER_H
#define RENDERER_H

#include "Simulation.hpp"
#include "PMat.hpp"
#include <glm/glm.hpp>

class Renderer {
public:
    Renderer();
    ~Renderer();
    void render(const Simulation& simulation);
    void adjustCameraToFit(const Simulation& simulation);

private:
    unsigned int shaderProgram;
    unsigned int vao, vbo;
    void init();
    void initShaders();
    int numSegments = 32;

    // Camera properties
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;  
    glm::vec3 targetPosition;
    glm::vec3 targetCenter;
    float lerpFactor = 0.02f;

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void initGrid();
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);
};

#endif // RENDERER_H