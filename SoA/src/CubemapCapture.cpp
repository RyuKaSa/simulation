#include "CubemapCapture.hpp"
#include "Camera.hpp"
#include "Simulation.hpp"
#include <iostream>

CubemapCapture::CubemapCapture()
    : fboID(0), rboID(0), cubemapTexID(0), cubemapSize(1024)
{
}

CubemapCapture::~CubemapCapture()
{
    if (fboID)
        glDeleteFramebuffers(1, &fboID);
    if (rboID)
        glDeleteRenderbuffers(1, &rboID);
    if (cubemapTexID)
        glDeleteTextures(1, &cubemapTexID);
}

void CubemapCapture::init(int cubeSize)
{
    cubemapSize = cubeSize;

    // If we already had something allocated, delete it first.
    if (fboID)
        glDeleteFramebuffers(1, &fboID);
    if (rboID)
        glDeleteRenderbuffers(1, &rboID);
    if (cubemapTexID)
        glDeleteTextures(1, &cubemapTexID);

    // Create the cubemap texture:
    glGenTextures(1, &cubemapTexID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexID);
    for (int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0, GL_RGBA, cubemapSize, cubemapSize,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Create and set up the FBO:
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    // Create a renderbuffer for depth
    glGenRenderbuffers(1, &rboID);
    glBindRenderbuffer(GL_RENDERBUFFER, rboID);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          cubemapSize, cubemapSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rboID);

    // No color attachment yet — we’ll attach each face as we render it.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CubemapCapture::renderToCubemap(SimulationBase &simulation,
                                     Camera &tempCameraForCapture,
                                     const glm::vec3 &camPos,
                                     Renderer &renderer)
{
    if (!fboID || !cubemapTexID)
    {
        std::cerr << "[CubemapCapture] Not initialized! Call init(...) first.\n";
        return;
    }

    // 6 directions for the cubemap
    static const glm::vec3 targets[6] = {
        glm::vec3(1, 0, 0),
        glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0),
        glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1),
        glm::vec3(0, 0, -1)
    };
    static const glm::vec3 ups[6] = {
        glm::vec3(0, -1, 0), // +X
        glm::vec3(0, -1, 0), // -X
        glm::vec3(0, 0, 1),  // +Y
        glm::vec3(0, 0, -1), // -Y
        glm::vec3(0, -1, 0), // +Z
        glm::vec3(0, -1, 0)  // -Z
    };

    // We'll render at 90° FOV in each direction to capture the entire surroundings
    float originalFov = tempCameraForCapture.getFOV();
    tempCameraForCapture.setFOV(90.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glViewport(0, 0, cubemapSize, cubemapSize);

    // For each face:
    for (int i = 0; i < 6; ++i)
    {
        // Attach the i-th face of the cubemap as color attachment 0
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            cubemapTexID,
            0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "[CubemapCapture] Framebuffer not complete for face " << i << "!\n";
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build a lookAt matrix for this face
        glm::mat4 view = glm::lookAt(camPos,
                                     camPos + targets[i],
                                     ups[i]);

        // Render using the provided renderer with our custom view/projection matrices
        renderer.renderWithMatrices(simulation, view, tempCameraForCapture.getProjectionMatrix(1.0f));
    } // end for each face

    // Restore original FOV
    tempCameraForCapture.setFOV(originalFov);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}