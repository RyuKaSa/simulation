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

    simulation.startAsyncUpdates();

    bool running = true;
    SDL_Event event;
    double renderDelta = 1.0 / 60.0;   // target 60 FPS rendering

    double physicsAccumulator = 0.0;
    double renderAccumulator = 0.0;
    double lastTime = getCurrentTime();

    while (running) {
        double frameStart = getCurrentTime();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);
        }

        // Process reset/throw ball requests BEFORE starting a new ImGui frame.
        if (gui.isResetRequested()) {
            simulation.stopAsyncUpdates();
            simulation.reset();
            renderer.cameraReset(simulation);
            gui.clearResetFlag();
            simulation.startAsyncUpdates();
        }
        if (gui.isThrowBallRequested()) {
            glm::vec3 camPos = renderer.getCameraPosition();
            glm::vec3 camDir = glm::normalize(renderer.getCameraTarget() - camPos);
            simulation.throwBall(camPos, camDir, 60.0f, 100.0f, glm::vec3(25.0f));
            gui.clearThrowBallFlag();
        }

        // Update global simulation parameters.
        simulation.setSpringConstant(gui.getSpringConstant());
        simulation.setDampingCoefficient(gui.getDampingCoefficient());
        simulation.setImpulseScaling(gui.getImpulseScaling());

        // Get performance counts.
        int numParticles = simulation.getSnapshotBalls().size();
        int numSprings = simulation.getSpringEndpoints().size() / 2;

        // Begin new ImGui frame.
        gui.newFrame();
        gui.draw();

        // --- Rendering ---
        double renderStart = getCurrentTime();
        renderer.render(simulation);  // In Renderer::render, use simulation.getSnapshotBalls()
        gui.render();
        SDL_GL_SwapWindow(window);
        double renderEnd = getCurrentTime();
        double renderFrameTime = renderEnd - renderStart;

        double frameEnd = getCurrentTime();
        double totalFrameTime = frameEnd - frameStart;
        double fps = (totalFrameTime > 0.0) ? 1.0 / totalFrameTime : 0.0;

        // Get the physics update time from the simulation thread.
        float physicsStepTime = simulation.lastPhysicsUpdateTime.load();
        int effectiveSteps = simulation.effectiveStepsPerSecond.load();

        // --- Update Performance Metrics in the GUI ---
        gui.setPerformanceMetrics(physicsStepTime,
                                  (float)renderFrameTime,
                                  (float)totalFrameTime,
                                  (float)fps,
                                  numParticles,
                                  numSprings,
                                  effectiveSteps);
    }

    simulation.stopAsyncUpdates();
    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}