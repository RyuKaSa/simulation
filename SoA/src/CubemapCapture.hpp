#ifndef CUBEMAP_CAPTURE_HPP
#define CUBEMAP_CAPTURE_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Renderer.hpp"

class Camera;        // Forward declare your Camera base class
class SimulationBase; // Forward declare your Simulation

class CubemapCapture {
public:
    CubemapCapture();
    ~CubemapCapture();

    // Initialize the cubemap FBO at a desired resolution (e.g., 1024x1024).
    // Call this once at startup or whenever the cubemap size needs to change.
    void init(int cubeSize);

    // Renders the scene from 6 directions into the cubemap.
    // 'camPos' is the position from which to capture.
    void renderToCubemap(SimulationBase& simulation,
                         Camera& tempCameraForCapture,
                         const glm::vec3& camPos,
                         Renderer& renderer);

    // Returns the ID of the cubemap texture (GL_TEXTURE_CUBE_MAP).
    unsigned int getCubemapID() const { return cubemapTexID; }

    // The size of each cube face (e.g., 1024 x 1024).
    int getSize() const { return cubemapSize; }

private:
    unsigned int fboID;
    unsigned int rboID;
    unsigned int cubemapTexID;
    int cubemapSize;
};

#endif // CUBEMAP_CAPTURE_HPP