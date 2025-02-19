#ifndef RENDERER_H
#define RENDERER_H

#include "Simulation.hpp"
#include "PMat.hpp"
#include <glm/glm.hpp>
#include "Shader.hpp"
#include "Renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Main render call
    void render(const Simulation& simulation);

    // Camera adjustments
    void adjustCameraToFit(const Simulation& simulation);
    void cameraReset(const Simulation& simulation);

    // Getters for camera position and target
    glm::vec3 getCameraPosition() const { return cameraPosition; }
    glm::vec3 getCameraTarget()   const { return cameraTarget; }

    // Rendering functions
    void renderBalls(const Simulation& simulation,
                     const glm::mat4& projection,
                     const glm::mat4& view);
    void renderHexTriangles(const Simulation& simulation,
                            const glm::mat4& projection,
                            const glm::mat4& view);
    void renderSprings(const Simulation& simulation,
                       const glm::mat4& projection,
                       const glm::mat4& view);
    void renderExternalCubes(const Simulation& simulation,
                             const glm::mat4& projection,
                             const glm::mat4& view);

private:
    Shader ballShader;
    Shader cubeShader;

    // Geometry for instanced balls
    unsigned int vao, vbo;
    unsigned int instancePosVBO, instanceColorVBO, instanceScaleVBO;

    void initGrid();
    void initCube();
    void initSprings();
    void initHexTriangles();
    void initBallGeometry();

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;
    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    // Spring geometry
    unsigned int springVAO, springVBO;

    // Hex geometry
    unsigned int hexTriVAO, hexTriVBO;

    // Cube geometry
    unsigned int cubeVAO, cubeVBO, cubeEBO;

    // Ball geometry
    int numSegments = 32;

    // Camera
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;
    glm::vec3 targetPosition;
    glm::vec3 targetCenter;
    float targetDistance;
    float lerpFactor = 0.03f;

    float minCameraDistance = 5.0f;
    float maxCameraDistance = 1000.0f;

    static const size_t maxInstances = 100000;
};

#endif // RENDERER_H