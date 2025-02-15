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
    void renderHexHitboxes(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);

    // getters for cam pos and direction
    glm::vec3 getCameraPosition() const { return cameraPosition; }
    glm::vec3 getCameraTarget() const { return cameraTarget; }

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
    float lerpFactor = 0.03f;

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void initGrid();
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    // Spring geometry
    unsigned int springVAO, springVBO;

    unsigned int hexVAO, hexVBO;

    // Maximum number of instances to support
    static const size_t maxInstances = 10000;
};

#endif // RENDERER_H