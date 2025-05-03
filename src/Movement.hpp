#ifndef MOVEMENT_HPP
#define MOVEMENT_HPP

#include <SDL.h>
#include <glm/glm.hpp>

// Forward declaration:
class FPSCamera;

class Movement {
public:
    // Pressed keys:
    static bool moveForward;
    static bool moveBackward;
    static bool moveLeft;
    static bool moveRight;
    static bool moveUp;
    static bool moveDown;

    // Speed parameters we can tweak:
    static float moveSpeed;     // base walking/flying speed
    static float sprintFactor; 

    static float mouseSensitivity;

    static void handleKeyEvent(const SDL_Event& e);

    static void updateFPSCamera(FPSCamera& camera, float dt);

    static void handleMouseMotion(const SDL_Event& e, FPSCamera& camera);
};

#endif // MOVEMENT_HPP