#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

#include "Simulation.hpp"
#include "Renderer.hpp"
#include "GUI.hpp"
#include "Scene.hpp"

double getCurrentTime() {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char* argv[])
{
    // --- SDL Init and GL context creation ---
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Balls Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL);
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

    // --- Create a single GUI instance (shared by both scenes) ---
    GUI gui(window, glContext);

    // --- Create the two derived simulations ---
    // Scene 1: Cloth
    ClothSimulation* clothSim = new ClothSimulation(&gui);

    // Scene 2: Environment
    EnvironmentSimulation* envSim = new EnvironmentSimulation(&gui);

    // --- Create Scenes from those simulations ---
    Scene scene1(clothSim);  // Scene that uses ClothSimulation
    Scene scene2(envSim);    // Scene that uses EnvironmentSimulation

    // Initialize each scene once.
    scene1.init();
    scene2.init();

    // Start with scene1 active (resumed). scene2 paused.
    scene1.resume();
    scene2.pause();

    int activeScene = 1; // 1 means scene1 is active, 2 means scene2 is active.
    bool isPaused = false; // whether the current active scene is paused

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            // Always forward events to ImGui so that it updates in both scenes
            gui.processEvent(event);

            // Toggle scene (switch between scene1 and scene2) with X
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x) {
                if (activeScene == 1) {
                    scene1.pause();
                    scene2.resume();
                    activeScene = 2;
                    // Reset pause state on scene switch
                    isPaused = false;
                    std::cout << "Switched to Scene 2" << std::endl;
                }
                else {
                    scene2.pause();
                    scene1.resume();
                    activeScene = 1;
                    isPaused = false;
                    std::cout << "Switched to Scene 1" << std::endl;
                }
            }

            // Toggle pause/resume for the current scene with C
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c) {
                if (!isPaused) {
                    if (activeScene == 1) {
                        scene1.pause();
                    } else {
                        scene2.pause();
                    }
                    isPaused = true;
                    std::cout << "Paused active scene" << std::endl;
                }
                else {
                    if (activeScene == 1) {
                        scene1.resume();
                    } else {
                        scene2.resume();
                    }
                    isPaused = false;
                    std::cout << "Resumed active scene" << std::endl;
                }
            }
        }

        // Let the user’s GUI settings update the active scene’s simulation
        if (activeScene == 1) {
            // For example, if reset is requested:
            if (gui.isResetRequested()) {
                scene1.simulation->stopAsyncUpdates();
                scene1.simulation->reset();
                scene1.renderer.cameraReset(*scene1.simulation);
                gui.clearResetFlag();
                if (!isPaused) scene1.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested()) {
                scene1.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            // Update the parameters from GUI
            scene1.simulation->setSpringConstant(gui.getSpringConstant());
            scene1.simulation->setDampingCoefficient(gui.getDampingCoefficient());
        }
        else {
            // Active scene is scene2
            if (gui.isResetRequested()) {
                scene2.simulation->stopAsyncUpdates();
                scene2.simulation->reset();
                scene2.renderer.cameraReset(*scene2.simulation);
                gui.clearResetFlag();
                if (!isPaused) scene2.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested()) {
                scene2.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            scene2.simulation->setSpringConstant(gui.getSpringConstant());
            scene2.simulation->setDampingCoefficient(gui.getDampingCoefficient());
        }

        // Render everything
        gui.newFrame();
        gui.draw();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render whichever scene is active
        if (activeScene == 1) {
            scene1.render(); // always render, even if paused
        } else {
            scene2.render();
        }

        // Render ImGui overlay
        gui.render();
        SDL_GL_SwapWindow(window);

        // Update performance metrics in the GUI
        if (activeScene == 1) {
            int numParticles = (int)scene1.simulation->getSoA().position.size();
            int numSprings   = (int)scene1.simulation->getSpringCount();
            double physicsStepTime = scene1.simulation->getLastPhysicsUpdateTime();
            int effectiveSteps = scene1.simulation->getEffectiveStepsPerSecond();

            // measure totalFrameTime or fps if you want
            float totalFrameTime = 0.0f;
            float fps = 0.0f;
            gui.setPerformanceMetrics((float)physicsStepTime,
                                      0.0f, // example "renderFrameTime"
                                      totalFrameTime,
                                      fps,
                                      numParticles,
                                      numSprings,
                                      effectiveSteps);
        }
        else {
            int numParticles = (int)scene2.simulation->getSoA().position.size();
            int numSprings   = (int)scene2.simulation->getSpringCount();
            double physicsStepTime = scene2.simulation->getLastPhysicsUpdateTime();
            int effectiveSteps = scene2.simulation->getEffectiveStepsPerSecond();

            float totalFrameTime = 0.0f;
            float fps = 0.0f;
            gui.setPerformanceMetrics((float)physicsStepTime,
                                      0.0f,
                                      totalFrameTime,
                                      fps,
                                      numParticles,
                                      numSprings,
                                      effectiveSteps);
        }
    }

    // Cleanup.
    scene1.shutdown();
    scene2.shutdown();
    // If you dynamically allocated clothSim/envSim in Scene:
    //   - either delete them here or from sceneX.shutdown() depending on your design
    delete clothSim;
    delete envSim;

    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}