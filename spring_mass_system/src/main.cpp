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
    double renderDelta = 1.0 / 60.0;   // target 60 FPS rendering

    double physicsAccumulator = 0.0;
    double renderAccumulator = 0.0;
    double lastTime = getCurrentTime();

    while (running) {
        double totalFrameStart = getCurrentTime();

        // Process events first.
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);
        }

        // --- Perform reset or throw ball BEFORE starting a new ImGui frame ---
        if (gui.isResetRequested()) {
            simulation.reset();
            renderer.cameraReset(simulation);
            gui.clearResetFlag();
        }
        if (gui.isThrowBallRequested()) {
            glm::vec3 camPos = renderer.getCameraPosition();
            glm::vec3 camDir = glm::normalize(renderer.getCameraTarget() - camPos);
            simulation.throwBall(camPos, camDir, 60.0f, 100.0f, glm::vec3(8.0f));
            gui.clearThrowBallFlag();
        }

        // Update global simulation parameters.
        simulation.setSpringConstant(gui.getSpringConstant());
        simulation.setDampingCoefficient(gui.getDampingCoefficient());
        simulation.setImpulseScaling(gui.getImpulseScaling());

        // Get object counts for performance info.
        int numParticles = simulation.getBalls().size();
        int numSprings = simulation.getSpringEndpoints().size() / 2;

        // Convert physics steps to a delta time.
        int physicsSteps = gui.getPhysicsSteps();
        double physicsDelta = 1.0 / double(physicsSteps);

        // Timing update.
        double now = getCurrentTime();
        double elapsed = now - lastTime;  // in seconds
        lastTime = now;
        physicsAccumulator += elapsed;
        renderAccumulator += elapsed;

        // --- Physics Updates ---
        double physicsStart = getCurrentTime();
        while (physicsAccumulator >= physicsDelta) {
            simulation.update(physicsDelta);
            physicsAccumulator -= physicsDelta;
        }
        double physicsEnd = getCurrentTime();
        double physicsStepTime = physicsEnd - physicsStart;

        // --- Begin a New ImGui Frame ---
        gui.newFrame();

        // Draw the GUI (including parameter controls and, later, performance metrics).
        gui.draw();

        // --- Rendering ---
        double renderFrameTime = 0.0;
        if (renderAccumulator >= renderDelta) {
            double renderStart = getCurrentTime();
            renderer.render(simulation);
            gui.render();
            SDL_GL_SwapWindow(window);
            double renderEnd = getCurrentTime();
            renderFrameTime = renderEnd - renderStart;
            renderAccumulator -= renderDelta;
        }
        else {
            // Always render GUI even if we're not swapping buffers.
            gui.render();
        }

        double totalFrameEnd = getCurrentTime();
        double totalFrameTime = totalFrameEnd - totalFrameStart;
        double fps = (totalFrameTime > 0.0) ? 1.0 / totalFrameTime : 0.0;

        // --- Update Performance Metrics in the GUI ---
        // (Assumes you have implemented GUI::setPerformanceMetrics as discussed.)
        gui.setPerformanceMetrics((float)physicsStepTime,
                                  (float)renderFrameTime,
                                  (float)totalFrameTime,
                                  (float)fps,
                                  numParticles,
                                  numSprings);
    }

    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}