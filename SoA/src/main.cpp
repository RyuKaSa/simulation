#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

#include "Renderer.hpp"
#include "Simulation.hpp"
#include "GUI.hpp"

double getCurrentTime() {
    return (double)SDL_GetPerformanceCounter()/ (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "Balls Simulation",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1600, 900,
        SDL_WINDOW_OPENGL
    );
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
    SDL_GL_SetSwapInterval(1);

    Renderer renderer;
    Simulation simulation;
    GUI gui(window, glContext);

    simulation.startAsyncUpdates();

    bool running = true;
    SDL_Event event;
    double lastTime = getCurrentTime();

    while (running) {
        double frameStart = getCurrentTime();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);
        }

        if (gui.isResetRequested()) {
            simulation.stopAsyncUpdates();
            simulation.reset();
            renderer.cameraReset(simulation);
            gui.clearResetFlag();
            simulation.startAsyncUpdates();
        }

        if (gui.isDropStructureRequested()) {
            simulation.dropStructure();
            gui.clearDropStructureFlag();
        }

        simulation.setSpringConstant(gui.getSpringConstant());
        simulation.setDampingCoefficient(gui.getDampingCoefficient());
        simulation.setImpulseScaling(gui.getImpulseScaling());

        int numParticles = (int)simulation.getSoA().position.size();
        int numSprings   = (int)simulation.getSpringCount();

        gui.newFrame();
        gui.draw();

        double renderStart = getCurrentTime();
        renderer.render(simulation);
        gui.render();
        SDL_GL_SwapWindow(window);
        double renderEnd = getCurrentTime();
        double renderFrameTime = (renderEnd - renderStart);

        double frameEnd = getCurrentTime();
        double totalFrameTime = (frameEnd - frameStart);
        double fps = (totalFrameTime>0.0)? (1.0/totalFrameTime) : 0.0;

        double physicsStepTime = simulation.lastPhysicsUpdateTime.load();
        int effectiveSteps = simulation.effectiveStepsPerSecond.load();

        gui.setPerformanceMetrics((float)physicsStepTime,
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