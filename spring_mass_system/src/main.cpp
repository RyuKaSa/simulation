#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

#include "Renderer.hpp"
#include "Simulation.hpp"
#include "GUI.hpp"

double getCurrentTime() {
    // Return seconds as a double
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[]) {
    // Initialize SDL and set OpenGL attributes for GLSL 330 Core
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Balls Simulation", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Create modules
    Renderer renderer;
    Simulation simulation;
    GUI gui(window, glContext);

    bool running = true;
    SDL_Event event;
    double renderDelta  = 1.0 / 60.0;   // 60 FPS rendering

    double physicsAccumulator = 0.0;
    double renderAccumulator  = 0.0;
    double lastTime = getCurrentTime(); // measure with SDL_GetPerformanceCounter, etc.

    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);
        }

        gui.newFrame();
        gui.draw(); // Draw GUI (slider for global parameter)

        // Check for a reset request.
        if (gui.isResetRequested()) {
            simulation.reset();
            renderer.cameraReset(simulation);
            // Clear the reset flag
            gui.clearResetFlag();
        }

        if (gui.isThrowCubeRequested()) {
            glm::vec3 camPos = renderer.getCameraPosition();
            // Compute the normalized camera direction:
            glm::vec3 camDir = glm::normalize(renderer.getCameraTarget() - camPos);
            simulation.throwCube(camPos, camDir, 30.0f, 1.0f, glm::vec3(1.0f));
            gui.clearThrowCubeFlag();
        }

        // Pass global parameter to simulation and update
        simulation.setSpringConstant(gui.getSpringConstant());
        simulation.setDampingCoefficient(gui.getDampingCoefficient());
        
        // Convert from steps-per-second to a delta time
        int physicsSteps = gui.getPhysicsSteps();
        double physicsDelta = 1.0 / double(physicsSteps);

        // Accumulator timing
        double now = getCurrentTime();
        double elapsed = now - lastTime;  // time in seconds
        lastTime = now;

        physicsAccumulator += elapsed;
        renderAccumulator  += elapsed;

        // 1) Do physics updates
        while (physicsAccumulator >= physicsDelta) {
            simulation.update(physicsDelta);
            physicsAccumulator -= physicsDelta;
        }

        // 2) Render at ~60 FPS if enough time has passed
        if (renderAccumulator >= renderDelta) {
            renderer.render(simulation);
            gui.render();  
            SDL_GL_SwapWindow(window);

            renderAccumulator -= renderDelta;
        }
        else {
            gui.render(); // for ensuring gui is rendered every loop iteration
        }
    }

    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}