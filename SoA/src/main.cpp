#include <SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <deque>
#include <numeric>  // for std::accumulate

#include "Simulation.hpp"
#include "Renderer.hpp"
#include "GUI.hpp"
#include "Scene.hpp"

// Returns the current time in seconds.
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
    // Vsync enabled to cap rendering to ~60 FPS.
    SDL_GL_SetSwapInterval(1);

    // --- Create a single GUI instance (shared by both scenes) ---
    GUI gui(window, glContext);

    // --- Create the two derived simulations ---
    // Scene 1: Cloth
    ClothSimulation* clothSim = new ClothSimulation(&gui);
    // Scene 2: Environment
    EnvironmentSimulation* envSim = new EnvironmentSimulation(&gui);

    // --- Create Scenes from those simulations ---
    Scene scene1(clothSim, new OrbitCamera());  // Scene that uses ClothSimulation
    Scene scene2(envSim, new FPSCamera);    // Scene that uses EnvironmentSimulation

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Set the resolution for each camera in your scenes:
    scene1.camera->setResolution(w, h);
    scene2.camera->setResolution(w, h);

    // Initialize each scene once.
    scene1.init();
    scene2.init();

    // Start with scene1 active and scene2 paused.
    scene1.resume();
    scene2.pause();

    int activeScene = 1; // 1 means scene1 is active, 2 means scene2 is active.
    bool isPaused = false; // whether the current active scene is paused

    double previousTime = getCurrentTime();
    // We expect about 16.67 ms per frame at 60 FPS.
    const double targetFrameDuration = 1.0 / 60.0; 

    // Queues for smoothing (store last 20 measurements)
    const size_t smoothingQueueSize = 20;
    std::deque<float> renderTimeQueue; // in milliseconds
    std::deque<float> frameTimeQueue;  // in milliseconds

    bool running = true;
    SDL_Event event;
    while (running)
    {
        double frameStartTime = getCurrentTime();  // Begin whole frame timing

        // Process events
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;

            gui.processEvent(event);

            // Toggle scene with 'X'
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x) {
                if (activeScene == 1) {
                    scene1.pause();
                    scene2.resume();
                    activeScene = 2;
                    isPaused = false;
                    std::cout << "Switched to Scene 2" << std::endl;
                } else {
                    scene2.pause();
                    scene1.resume();
                    activeScene = 1;
                    isPaused = false;
                    std::cout << "Switched to Scene 1" << std::endl;
                }
            }

            // Toggle pause/resume with 'C'
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c) {
                if (!isPaused) {
                    if (activeScene == 1)
                        scene1.pause();
                    else
                        scene2.pause();
                    isPaused = true;
                    std::cout << "Paused active scene" << std::endl;
                } else {
                    if (activeScene == 1)
                        scene1.resume();
                    else
                        scene2.resume();
                    isPaused = false;
                    std::cout << "Resumed active scene" << std::endl;
                }
            }
        }

        // Update simulation parameters (physics updates run asynchronously)
        if (activeScene == 1) {
            if (gui.isResetRequested()) {
                scene1.simulation->stopAsyncUpdates();
                scene1.simulation->reset();
                // scene1.renderer.cameraReset(*scene1.simulation);
                gui.clearResetFlag();
                if (!isPaused)
                    scene1.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested()) {
                scene1.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            scene1.simulation->setSpringConstant(gui.getSpringConstant());
            scene1.simulation->setDampingCoefficient(gui.getDampingCoefficient());
            OrbitCamera* orbitCam = dynamic_cast<OrbitCamera*>(scene1.camera);
            if (orbitCam) {
                orbitCam->setZoom(gui.getCameraZoom());
            }
        } else {
            if (gui.isResetRequested()) {
                scene2.simulation->stopAsyncUpdates();
                scene2.simulation->reset();
                // scene2.renderer.cameraReset(*scene2.simulation);
                gui.clearResetFlag();
                if (!isPaused)
                    scene2.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested()) {
                scene2.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            scene2.simulation->setSpringConstant(gui.getSpringConstant());
            scene2.simulation->setDampingCoefficient(gui.getDampingCoefficient());
        }

        // Prepare and draw ImGui content.
        gui.newFrame();
        gui.draw();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Measure Render Time ---
        double renderStartTime = getCurrentTime();
        if (activeScene == 1)
            scene1.render();
        else
            scene2.render();
        double renderEndTime = getCurrentTime();
        float renderFrameTimeMs = (float)((renderEndTime - renderStartTime) * 1000.0f);
        // (If this number seems too high, check what scene.render() is doing.)

        // Render ImGui overlay and swap buffers
        gui.render();
        SDL_GL_SwapWindow(window);

        // --- Measure Total Frame Time ---
        double frameEndTime = getCurrentTime();
        float totalFrameTimeMs = (float)((frameEndTime - frameStartTime) * 1000.0f);

        // --- Update smoothing queues ---
        renderTimeQueue.push_back(renderFrameTimeMs);
        if (renderTimeQueue.size() > smoothingQueueSize)
            renderTimeQueue.pop_front();

        frameTimeQueue.push_back(totalFrameTimeMs);
        if (frameTimeQueue.size() > smoothingQueueSize)
            frameTimeQueue.pop_front();

        // Compute averages
        float avgRenderTime = std::accumulate(renderTimeQueue.begin(), renderTimeQueue.end(), 0.0f) / renderTimeQueue.size();
        float avgFrameTime  = std::accumulate(frameTimeQueue.begin(), frameTimeQueue.end(), 0.0f) / frameTimeQueue.size();
        float avgFPS = (avgFrameTime > 0.0f) ? (1000.0f / avgFrameTime) : 0.0f;

        // --- Update performance metrics in the GUI ---
        if (activeScene == 1) {
            int numParticles = (int)scene1.simulation->getSoA().position.size();
            int numSprings   = (int)scene1.simulation->getSpringCount();
            double physicsStepTime = scene1.simulation->getLastPhysicsUpdateTime(); // in ms, assumed
            int effectiveSteps = scene1.simulation->getEffectiveStepsPerSecond();

            gui.setPerformanceMetrics((float)physicsStepTime,
                                      avgRenderTime,
                                      avgFrameTime,
                                      avgFPS,
                                      numParticles,
                                      numSprings,
                                      effectiveSteps);
        } else {
            int numParticles = (int)scene2.simulation->getSoA().position.size();
            int numSprings   = (int)scene2.simulation->getSpringCount();
            double physicsStepTime = scene2.simulation->getLastPhysicsUpdateTime();
            int effectiveSteps = scene2.simulation->getEffectiveStepsPerSecond();

            gui.setPerformanceMetrics((float)physicsStepTime,
                                      avgRenderTime,
                                      avgFrameTime,
                                      avgFPS,
                                      numParticles,
                                      numSprings,
                                      effectiveSteps);
        }

        // Optionally, print the total frame time for debugging.
        // std::cout << "Frame time: " << totalFrameTimeMs << " ms" << std::endl;
    }

    // Cleanup.
    scene1.shutdown();
    scene2.shutdown();
    delete clothSim;
    delete envSim;

    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}