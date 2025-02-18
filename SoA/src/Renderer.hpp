#ifndef RENDERER_H
#define RENDERER_H

#include "Simulation.hpp"
#include "PMat.hpp"
#include <glm/glm.hpp>
#include "Shader.hpp"   // New: our shader management class

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // Main render call.
    void render(const Simulation& simulation);
    
    // Camera adjustments.
    void adjustCameraToFit(const Simulation& simulation);
    void cameraReset(const Simulation& simulation);

    // Getters for camera position and target.
    glm::vec3 getCameraPosition() const { return cameraPosition; }
    glm::vec3 getCameraTarget() const { return cameraTarget; }

    // Rendering functions for various elements.
    void renderBalls(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);
    void renderHexTriangles(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);
    void renderSprings(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);
    void renderExternalCubes(const Simulation& simulation, const glm::mat4& projection, const glm::mat4& view);

private:
    // Instead of raw shader program IDs, we now use Shader objects.
    Shader ballShader;  // For instanced ball, grid, springs, and hexagon triangles.
    Shader cubeShader;  // For external cubes (wireframe).

    // Geometry for the instanced balls.
    unsigned int vao, vbo;
    unsigned int instancePosVBO, instanceColorVBO, instanceScaleVBO;

    void init();      // Initializes ball geometry, instance buffers, etc.
    void initGrid();  // Initializes grid geometry.

    // Grid geometry.
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    // Spring geometry.
    unsigned int springVAO, springVBO;

    // Hexagon triangles geometry.
    unsigned int hexTriVAO, hexTriVBO;

    // Ball geometry parameters.
    int numSegments = 32;

    // Camera properties.
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;
    glm::vec3 targetPosition;
    glm::vec3 targetCenter;
    float targetDistance;
    float lerpFactor = 0.03f;

    float minCameraDistance = 5.0f;
    float maxCameraDistance = 1000.0f;

    // Maximum number of instances.
    static const size_t maxInstances = 100000;
};

#endif // RENDERER_H