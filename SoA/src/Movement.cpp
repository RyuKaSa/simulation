#include "Movement.hpp"
#include "Camera.hpp"
#include <algorithm> // for std::clamp

bool Movement::moveForward  = false;
bool Movement::moveBackward = false;
bool Movement::moveLeft     = false;
bool Movement::moveRight    = false;
bool Movement::moveUp       = false;
bool Movement::moveDown     = false;

float Movement::moveSpeed      = 5.0f;
float Movement::sprintFactor   = 2.0f;
float Movement::mouseSensitivity = 0.1f;

void Movement::handleKeyEvent(const SDL_Event& e)
{
    bool isKeyDown = (e.type == SDL_KEYDOWN);

    switch (e.key.keysym.sym) {
        case SDLK_z:
            moveForward = isKeyDown;
            break;
        case SDLK_s:
            moveBackward = isKeyDown;
            break;
        case SDLK_q:
            moveLeft = isKeyDown;
            break;
        case SDLK_d:
            moveRight = isKeyDown;
            break;
        case SDLK_SPACE:
            moveUp = isKeyDown;
            break;
        case SDLK_LSHIFT:
            moveDown = isKeyDown;
            break;
        default:
            break;
    }
}

void Movement::handleMouseMotion(const SDL_Event& e, FPSCamera& camera)
{
    // e.motion.xrel, e.motion.yrel are the *relative* mouse motion (in px)
    // We'll multiply by mouseSensitivity to rotate the camera's yaw/pitch.
    float deltaX = (float)e.motion.xrel * mouseSensitivity;
    float deltaY = (float)e.motion.yrel * mouseSensitivity;

    // camera.getYaw() / camera.getPitch() are in degrees, so just add deltas:
    float yaw   = camera.getYaw();
    float pitch = camera.getPitch();

    yaw   += deltaX;   // rotate left/right
    pitch -= deltaY;   // rotate up/down

    // Now clamp pitch in [-85..85]
    if (pitch > 85.0f)  pitch = 85.0f;
    if (pitch < -85.0f) pitch = -85.0f;

    // Update the camera’s angles
    camera.setYawPitch(yaw, pitch);
}

void Movement::updateFPSCamera(FPSCamera& camera, float dt)
{
    // Get the raw forward vector from the camera.
    glm::vec3 rawForward = camera.getForward();
    // Project onto horizontal plane (ignore vertical component).
    glm::vec3 forward = glm::normalize(glm::vec3(rawForward.x, 0.0f, rawForward.z));
    
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    // Compute right using the horizontal forward vector.
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    
    // Calculate horizontal velocity based on ZQSD (WASD) input.
    glm::vec3 horizontalVelocity(0.0f);
    if (moveForward)
        horizontalVelocity += forward;
    if (moveBackward)
        horizontalVelocity -= forward;
    if (moveLeft)
        horizontalVelocity -= right;
    if (moveRight)
        horizontalVelocity += right;
    
    // Calculate vertical velocity based solely on SPACE/LSHIFT.
    glm::vec3 verticalVelocity(0.0f);
    if (moveUp)
        verticalVelocity += up;
    if (moveDown)
        verticalVelocity -= up;
    
    // Normalize each component if necessary and scale by speed and dt.
    if (glm::length(horizontalVelocity) > 0.0001f) {
        horizontalVelocity = glm::normalize(horizontalVelocity) * moveSpeed * dt;
    }
    if (glm::length(verticalVelocity) > 0.0001f) {
        verticalVelocity = glm::normalize(verticalVelocity) * moveSpeed * dt;
    }
    
    // Update the camera position by adding horizontal and vertical movements.
    camera.setPosition(camera.getPosition() + horizontalVelocity + verticalVelocity);
}