#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class SimulationBase;

class Camera {
public:
    Camera();
    virtual ~Camera();

    // Update the camera’s internal parameters each frame or on user input
    virtual void update(float deltaTime) = 0;

    void setResolution(int width, int height);

    // Called for perspective or orthographic
    // We pass the aspect ratio so the camera can compute the correct projection
    virtual glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // Returns the camera’s view matrix
    virtual glm::mat4 getViewMatrix() const = 0;

    void beginRender();
    void endRender();

    // The texture to which the camera has rendered.
    // we can sample this in a post-processing pass.
    unsigned int getRenderTexture() const { return renderTextureID; }

    // Setters for standard perspective parameters:
    void setFOV(float newFov)       { fov = newFov; }
    void setNearPlane(float np)     { nearPlane = np; }
    void setFarPlane(float fp)      { farPlane  = fp; }

    float getFOV() const { return fov; }

protected:
    // Perspective camera defaults
    float fov;
    float nearPlane;
    float farPlane;

    // Framebuffer, color texture, etc. for off-screen rendering
    unsigned int fboID;
    unsigned int renderTextureID;
    unsigned int rboID;

    int renderWidth;
    int renderHeight;

    // Helper to create and configure the FBO
    void initFramebuffer(int width, int height);
};

//
// OrbitCamera: typical "orbit around a target" style (Scene 1).
//
class OrbitCamera : public Camera {
    public:
        OrbitCamera();
        virtual ~OrbitCamera() override;
    
        void setTarget(const glm::vec3& t);
        void setDistance(float d);
    
        // Update method to handle user input, etc.
        virtual void update(float deltaTime) override;
        virtual glm::mat4 getViewMatrix() const override;
    
        // Expose yaw/pitch for external access.
        void setYaw(float y)   { yaw = y; }
        void setPitch(float p) { pitch = p; }
    
        // Adjusts the camera's target and distance to fit the simulation's structure.
        // The target (look-at point) is updated quickly, while the camera's position/distance is interpolated more slowly.
        void adjustToFit(const SimulationBase& simulation);
        void forceFit(const SimulationBase& simulation);
    
        // Set the offset vector. This offset is added to the computed camera position.
        // For example, an offset of (2, 1, 0) will shift the camera 2 units to the right and 1 unit upward.
        void setPositionOffset(const glm::vec3& off) { offset = off; }
        const glm::vec3& getPositionOffset() const { return offset; }

        void setZoom(float z) { zoom = z; }
        float getZoom() const { return zoom; }
    
    private:
        // Current parameters used in view matrix calculation:
        glm::vec3 target;   // where the camera is looking
        float distance;     // current distance from the target
        float yaw;          // horizontal angle (in degrees)
        float pitch;        // vertical angle (in degrees)
    
        // Clamping for camera distance:
        float minCameraDistance = 0.1f;
        float maxCameraDistance = 1000.0f;
    
        // Auto-fit computed values:
        glm::vec3 targetCenter;   // ideal target computed from the simulation (fast lerp)
        glm::vec3 targetPosition; // ideal camera position computed from the simulation
        float targetDistance;     // ideal distance computed from the simulation
    
        // Lerp factors (adjustable for responsiveness):
        // Fast interpolation for the target center.
        float targetLerpFactor = 0.2f;
        // Slow interpolation for the camera's distance/position.
        float positionLerpFactor = 0.05f;
    
        // Additional offset between the computed camera position and the actual camera position.
        // This allows you to create diagonal or off-center shots.
        glm::vec3 offset = glm::vec3(0.0f); // default no offset
        float zoom = 1.0f;
    };

//
// FPSCamera: first-person flight, classic "lookAt position + forward direction" (Scene 2).
//
class FPSCamera : public Camera {
    public:
        FPSCamera();
        virtual ~FPSCamera() override;
        
        virtual void update(float deltaTime) override;
        virtual glm::mat4 getViewMatrix() const override;
        
        void setPosition(const glm::vec3& p);
        
        // Set yaw and pitch and immediately update the forward vector.
        void setYawPitch(float newYaw, float newPitch) {
            yaw = newYaw;
            pitch = newPitch;
            updateForward();
        }
        float getYaw() const { return yaw; }
        float getPitch() const { return pitch; }
        glm::vec3 getForward() const { return forward; }
        glm::vec3 getPosition() const { return position; }
        
    private:
        glm::vec3 position;
        float yaw;    // in degrees
        float pitch;  // in degrees
        glm::vec3 forward;
        glm::vec3 up;
        
        // Recalculate the forward vector from yaw and pitch.
        void updateForward() {
            forward.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
            forward.y = sinf(glm::radians(pitch));
            forward.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
            forward = glm::normalize(forward);
        }
};