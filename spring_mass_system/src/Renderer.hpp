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
    void cameraReset(const Simulation& simulation);

private:
    unsigned int shaderProgram;
    unsigned int vao, vbo;
    unsigned int instanceVBO;
    
    void init();
    void initShaders();
    int numSegments = 32;

    // Cached uniform locations
    int mvpLoc, modelLoc, colorLoc, useInstanceLoc, gridSpacingLoc;

    // Camera properties
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;  
    glm::vec3 targetPosition;
    glm::vec3 targetCenter;
    float targetDistance;
    float lerpFactor = 0.06f;

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void initGrid();
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    // Spring geometry
    unsigned int springVAO, springVBO;
};

#endif // RENDERER_H