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

    // Getters for camera position and target
    glm::vec3 getCameraPosition() const { return cameraPosition; }
    glm::vec3 getCameraTarget() const { return cameraTarget; }

    // New: Render hexagon triangles
    void renderBalls(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);
    void renderHexTriangles(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);
    void renderSprings(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);

private:
    unsigned int shaderProgram;
    unsigned int vao, vbo;
    // Remove the old instanceVBO and add separate buffers:
    unsigned int instancePosVBO, instanceColorVBO, instanceScaleVBO;

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

    float minCameraDistance = 5.0f;
    float maxCameraDistance = 1000.0f;

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void initGrid();
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    // Spring geometry
    unsigned int springVAO, springVBO;

    // Hexagon triangles geometry for rendering
    unsigned int hexTriVAO, hexTriVBO;

    // Maximum number of instances to support
    static const size_t maxInstances = 10000;
};

#endif // RENDERER_H