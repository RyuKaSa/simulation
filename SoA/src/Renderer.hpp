#ifndef RENDERER_H
#define RENDERER_H

#include "Simulation.hpp"
#include "PMat.hpp"
#include <glm/glm.hpp>
#include "Shader.hpp"
#include <glad/glad.h>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include "Camera.hpp"
#include "FullscreenQuad.hpp"

class Renderer {
public:
    // Constructor takes a Simulation reference for geometry initialization.
    Renderer(const SimulationBase& simulation);
    ~Renderer();

    FullscreenQuad* postProcessQuad;

    // Main render call
    void render(const SimulationBase& simulation);

    // Attach a camera (OrbitCamera, FPSCamera, etc.)
    void setCamera(Camera* cam) { camera = cam; }

    void initTripleGrid(const SimulationBase& simulation);
    void initHorizontalGrid(float gridExtent, float spacing);

    // Rendering functions
    void renderBalls(const SimulationBase& simulation,
                     const glm::mat4& projection,
                     const glm::mat4& view);
    void renderHexTriangles(const SimulationBase& simulation,
                            const glm::mat4& projection,
                            const glm::mat4& view);
    void renderSprings(const SimulationBase& simulation,
                       const glm::mat4& projection,
                       const glm::mat4& view);
    void renderExternalCubes(const SimulationBase& simulation,
                             const glm::mat4& projection,
                             const glm::mat4& view);

    void renderGrid(const glm::mat4& projection, const glm::mat4& view);

    void renderWithMatrices(const SimulationBase &simulation, const glm::mat4 &view, const glm::mat4 &projection);
    
private:
    Shader ballShader;
    Shader cubeShader;
    Shader springShader;
    Shader meshShader;
    Shader gridShader;

    // Our camera pointer
    Camera* camera;

    // Geometry for instanced balls
    unsigned int vao, vbo;
    unsigned int instancePosVBO, instanceColorVBO, instanceScaleVBO;

    void initCube();
    void initSprings();
    void initHexTriangles();
    void initBallGeometry();

    // Grid geometry
    unsigned int gridVAO, gridVBO;
    int gridVertexCount;

    // Spring geometry
    unsigned int springVAO, springVBO;

    // Hex geometry
    unsigned int hexTriVAO, hexTriVBO;

    // Cube geometry
    unsigned int cubeVAO, cubeVBO, cubeEBO;

    // Ball geometry
    int numSegments = 32;

    static const size_t maxInstances = 100000;
};

#endif // RENDERER_H