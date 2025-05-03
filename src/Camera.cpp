#include "Camera.hpp"
#include <glad/glad.h>  // for glGenFramebuffers etc.
#include <iostream>
#include <cmath>

#include "Simulation.hpp"

// ------------------ Base Camera Implementation ------------------ //

Camera::Camera()
    : fov(45.0f)
    , nearPlane(0.1f)
    , farPlane(1000.0f)
    , fboID(0)
    , renderTextureID(0)
    , rboID(0)
    , renderWidth(1280)
    , renderHeight(720)
{

    // Initialize an FBO and texture so we can draw the scene onto it
    initFramebuffer(renderWidth, renderHeight);
}

Camera::~Camera() {
    // Cleanup any allocated OpenGL objects
    if(fboID) glDeleteFramebuffers(1, &fboID);
    if(renderTextureID) glDeleteTextures(1, &renderTextureID);
    if(rboID) glDeleteRenderbuffers(1, &rboID);
}

void Camera::setResolution(int width, int height) {
    renderWidth = width;
    renderHeight = height;
    // Delete existing FBO objects before reinitializing.
    if(fboID) glDeleteFramebuffers(1, &fboID);
    if(renderTextureID) glDeleteTextures(1, &renderTextureID);
    if(rboID) glDeleteRenderbuffers(1, &rboID);
    initFramebuffer(renderWidth, renderHeight);
}

void Camera::beginRender() {
    // Bind our off-screen FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    // Set the viewport to the texture size
    glViewport(0, 0, renderWidth, renderHeight);
    // Clear out old color/depth
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Camera::endRender() {
    // Unbind the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    // Simple perspective
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void Camera::initFramebuffer(int width, int height) {
    // Create the framebuffer:
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    // Create a texture for color:
    glGenTextures(1, &renderTextureID);
    glBindTexture(GL_TEXTURE_2D, renderTextureID);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        width, height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, renderTextureID, 0
    );

    // Create a renderbuffer for depth/stencil:
    glGenRenderbuffers(1, &rboID);
    glBindRenderbuffer(GL_RENDERBUFFER, rboID);
    glRenderbufferStorage(
        GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
        width, height
    );
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, rboID
    );

    // Check for completeness:
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Camera framebuffer not complete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// ------------------ OrbitCamera Implementation ------------------ //

OrbitCamera::OrbitCamera()
    : Camera()  // Call base constructor.
    , target(0.0f, 0.0f, 0.0f)
    , distance(1.0f)
    , yaw(0.0f)
    , pitch(0.0f)
    , minCameraDistance(0.1f)
    , maxCameraDistance(1000.0f)
    , targetCenter(0.0f, 0.0f, 0.0f)
    , targetDistance(1.0f)
    , targetLerpFactor(0.1f)     // fast lerp for target updates
    , positionLerpFactor(0.01f)    // slow lerp for distance/position updates
    , offset(-0.2f, 1.8f, 4.5f)
    , zoom(0.5f)
{
    initFramebuffer(renderWidth, renderHeight);
}

OrbitCamera::~OrbitCamera() {}

void OrbitCamera::setTarget(const glm::vec3 &t) {
    target = t;
}

void OrbitCamera::setDistance(float d) {
    distance = (d < 0.01f) ? 0.01f : d;
}

void OrbitCamera::update(float /*deltaTime*/) {
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    float radYaw   = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    // Compute the directional component.
    glm::vec3 direction = glm::vec3(std::cos(radYaw) * std::cos(radPitch),
                                    std::sin(radPitch),
                                    std::sin(radYaw) * std::cos(radPitch));
    // Compute displacement: distance * direction, then add the user offset.
    glm::vec3 disp = distance * direction + offset;
    // Apply the zoom factor uniformly to the entire displacement.
    glm::vec3 pos = target + zoom * disp;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    return glm::lookAt(pos, target, up);
}

void OrbitCamera::adjustToFit(const SimulationBase& simulation) {
    // Compute bounding box from every third structure particle.
    std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

    size_t n = positions.size();
    glm::dvec3 minPos = positions[0];
    glm::dvec3 maxPos = positions[0];

    for (size_t i = 0; i < n; i += 3) {
        minPos = glm::min(minPos, positions[i]);
        maxPos = glm::max(maxPos, positions[i]);
    }
    if ((n - 1) % 3 != 0) {
        minPos = glm::min(minPos, positions.back());
        maxPos = glm::max(maxPos, positions.back());
    }
    glm::dvec3 center = (minPos + maxPos) * 0.5;
    double maxExtent = glm::length(maxPos - minPos);
    if (maxExtent < 0.0001)
        maxExtent = 1.0;

    // Compute the ideal target (simulation center).
    glm::vec3 idealTarget = glm::vec3(center);
    targetCenter = glm::mix(targetCenter, idealTarget, targetLerpFactor);

    // Compute the ideal distance, scaled by the zoom factor.
    float idealDistance = static_cast<float>(glm::clamp(maxExtent * zoom,
                                  (double)minCameraDistance,
                                  (double)maxCameraDistance));
    targetDistance = glm::mix(targetDistance, idealDistance, targetLerpFactor);

    // Update actual distance with a slower interpolation.
    distance = glm::mix(distance, targetDistance, positionLerpFactor);

    // Finally update the camera target.
    setTarget(targetCenter);
    setDistance(distance);
}

void OrbitCamera::forceFit(const SimulationBase& simulation) {
    // Compute bounding box from every third structure particle.
    std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
    if (positions.empty()) return;

    size_t n = positions.size();
    glm::dvec3 minPos = positions[0];
    glm::dvec3 maxPos = positions[0];

    for (size_t i = 0; i < n; i += 3) {
        minPos = glm::min(minPos, positions[i]);
        maxPos = glm::max(maxPos, positions[i]);
    }
    if ((n - 1) % 3 != 0) {
        minPos = glm::min(minPos, positions.back());
        maxPos = glm::max(maxPos, positions.back());
    }
    
    // Determine the center and maximum extent of the bounding box.
    glm::dvec3 center = (minPos + maxPos) * 0.5;
    double maxExtent = glm::length(maxPos - minPos);
    if (maxExtent < 0.0001)
        maxExtent = 1.0;

    // Set the camera target directly to the computed center.
    glm::vec3 idealTarget = glm::vec3(center);
    targetCenter = idealTarget;

    // Calculate the ideal distance scaled by the zoom factor,
    // clamped between the minimum and maximum allowed distances.
    float idealDistance = static_cast<float>(glm::clamp(maxExtent * zoom,
                                  (double)minCameraDistance,
                                  (double)maxCameraDistance));
    // Set the distance immediately.
    targetDistance = idealDistance;
    distance = idealDistance;

    // Update the camera parameters.
    setTarget(targetCenter);
    setDistance(distance);
}


// ------------------ FPSCamera Implementation ------------------ //

FPSCamera::FPSCamera()
    : position(0.0f, 2.0f, 5.0f),
      yaw(0.0f),
      pitch(0.0f),
      up(0.0f, 1.0f, 0.0f)
{
    updateForward();
}

FPSCamera::~FPSCamera() {}

void FPSCamera::update(float deltaTime)
{
    updateForward();
}

glm::mat4 FPSCamera::getViewMatrix() const
{
    return glm::lookAt(position, position + forward, up);
}

void FPSCamera::setPosition(const glm::vec3& p)
{
    position = p;
}