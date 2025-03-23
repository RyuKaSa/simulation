#include <SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <deque>
#include <numeric> // for std::accumulate

#include "imgui.h"
#include "Simulation.hpp"
#include "Renderer.hpp"
#include "GUI.hpp"
#include "Scene.hpp"
#include "Movement.hpp"
#include "BlockWorld.hpp"
#include "BlockPlacement.hpp"
#include "CubemapCapture.hpp"
#include "EquirectangularConverter.hpp"
#include "FivePointDistortion.hpp"

// Returns the current time in seconds.
double getCurrentTime()
{
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char *argv[])
{
    // --- SDL Init and GL context creation ---
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = SDL_CreateWindow("Balls Simulation",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext)
    {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    // Vsync enabled to cap rendering to ~60 FPS.
    SDL_GL_SetSwapInterval(1);
    // enable fps mouse movement
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // --- Create a single GUI instance (shared by both scenes) ---
    GUI gui(window, glContext);

    // --- Create the two derived simulations ---
    // Scene 1: Cloth
    ClothSimulation *clothSim = new ClothSimulation(&gui);
    // Scene 2: Environment
    EnvironmentSimulation *envSim = new EnvironmentSimulation(&gui);

    // --- Create Scenes from those simulations ---
    Scene scene1(clothSim, new OrbitCamera()); // Scene that uses ClothSimulation
    Scene scene2(envSim, new FPSCamera);       // Scene that uses EnvironmentSimulation

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Set the resolution for each camera in your scenes:
    scene1.camera->setResolution(w, h);
    scene2.camera->setResolution(w, h);

    // Initialize each scene once.
    scene1.init();
    scene1.renderer.initTripleGrid(*scene1.simulation);
    scene2.init();
    scene2.renderer.initHorizontalGrid(50.0f, 0.5f);

    // each block is 1 unit, and grid spans ±50 units.
    BlockWorld blockWorld(1.0f, 50.0f, envSim);

    // Template 1: a 3x3 square on the ground.
    std::vector<GridCoord> template1 = {
        {-8, 7, 13},
        {8, 7, 13},
        {8, 7, -3},
        {-8, 7, -3},
        {8, 0, -3},
        {-8, 0, -3},
        {8, 0, 13},
        {-8, 0, 13}
    };

    // Template 2: a plus sign.
    std::vector<GridCoord> template2 = {
        {-8, 6, 13},
        {8, 5, 13},
        {8, 1, -3},
        {-8, 0, -3}
    };

    CubemapCapture cubeCapture;
    EquirectangularConverter equiConverter;
    FivePointDistortion distortionPass;

    cubeCapture.init(w * 2);
    equiConverter.init(w, h);
    distortionPass.init(w * 2, h * 2);

    // Start with scene1 active and scene2 paused.
    scene1.resume();
    scene2.pause();

    int activeScene = 1;   // 1 means scene1 is active, 2 means scene2 is active.
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
        double frameStartTime = getCurrentTime(); // Begin whole frame timing

        while (SDL_PollEvent(&event))
        {
            // Quit event.
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);

            // Toggle scenes with the 'X' key.
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x)
            {
                if (activeScene == 1)
                {
                    scene1.pause();
                    scene2.resume();
                    activeScene = 2;
                    isPaused = false;
                    std::cout << "Switched to Scene 2" << std::endl;
                }
                else
                {
                    scene2.pause();
                    scene1.resume();
                    activeScene = 1;
                    isPaused = false;
                    std::cout << "Switched to Scene 1" << std::endl;
                }
            }

            // Toggle pause/resume with the 'C' key.
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c)
            {
                if (!isPaused)
                {
                    if (activeScene == 1)
                        scene1.pause();
                    else
                        scene2.pause();
                    isPaused = true;
                    std::cout << "Paused active scene" << std::endl;
                }
                else
                {
                    if (activeScene == 1)
                        scene1.resume();
                    else
                        scene2.resume();
                    isPaused = false;
                    std::cout << "Resumed active scene" << std::endl;
                }
            }

            // Handle key events via Movement module.
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                Movement::handleKeyEvent(event);

            // Handle mouse motion (only for Scene 2 when active and not paused).
            if (event.type == SDL_MOUSEMOTION && activeScene == 2 && !isPaused)
            {
                FPSCamera *fpsCam = dynamic_cast<FPSCamera *>(scene2.camera);
                if (fpsCam)
                    Movement::handleMouseMotion(event, *fpsCam);
            }

            if (activeScene == 2 && event.type == SDL_MOUSEBUTTONDOWN)
            {
                // If ImGui is handling mouse input, ignore these clicks.
                if (ImGui::GetIO().WantCaptureMouse)
                    continue;
            
                FPSCamera* fpsCam = dynamic_cast<FPSCamera*>(scene2.camera);
                if (fpsCam) {
                    glm::vec3 rayOrigin = fpsCam->getPosition();
                    glm::vec3 rayDir    = fpsCam->getForward();
            
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        GridCoord targetCoord;
                        if (GetBlockAdditionCoord(rayOrigin, rayDir, blockWorld, *scene2.simulation, targetCoord)) {
                            if (!blockWorld.hasBlock(targetCoord))
                            {
                                blockWorld.addBlock(targetCoord.x, targetCoord.y, targetCoord.z);
                                blockWorld.updateSimulation(scene2.simulation->getSoAReference(),
                                                            scene2.simulation->getSpringsReference());
                            }
                        }
                    }
                    else if (event.button.button == SDL_BUTTON_RIGHT) {
                        GridCoord removeCoord;
                        if (GetBlockRemovalCoord(rayOrigin, rayDir, blockWorld, *scene2.simulation, removeCoord)) {
                            if (blockWorld.hasBlock(removeCoord)) {
                                blockWorld.removeBlock(removeCoord);
                                blockWorld.updateSimulation(scene2.simulation->getSoAReference(),
                                                            scene2.simulation->getSpringsReference());
                            }
                        }
                    }
                }
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
            {
                // Only place cloth in Scene 2.
                if (activeScene == 2)
                {
                    std::cout << "Placing cloth at target block..." << std::endl;
                    FPSCamera* fpsCam = dynamic_cast<FPSCamera*>(scene2.camera);
                    if (!fpsCam)
                        continue;
            
                    glm::vec3 rayOrigin = fpsCam->getPosition();
                    glm::vec3 rayDir    = fpsCam->getForward();
            
                    // Use the same raycasting function to get the grid coordinate.
                    GridCoord targetCoord;
                    if (GetBlockAdditionCoord(rayOrigin, rayDir, blockWorld, *scene2.simulation, targetCoord))
                    {
                        // Cast simulation pointer to EnvironmentSimulation.
                        EnvironmentSimulation* envSim = dynamic_cast<EnvironmentSimulation*>(scene2.simulation);
                        if (envSim)
                        {
                            // Pull cloth parameters from sharedParams.
                            int gridSize = envSim->sharedParams.gridSize;
                            double springRestLength = envSim->sharedParams.springRestLength;
                            // For cell size and layer spacing, you can choose to use a shared parameter or a constant.
                            double cellSize = 0.03;     // e.g., similar to the hexgrid test.
                            int nLayers = 2;            // Number of layers (could also be a shared param)
                            double layerSpacing = 0.03; // Adjust as desired.
            
                            // Create the cloth with its center at the target block.
                            envSim->createMultiLayerSquareGridWithDiagonalsCentered(
                                targetCoord.x, targetCoord.y, targetCoord.z,
                                gridSize, nLayers, cellSize, layerSpacing, springRestLength);
            
                            // After creation, update spring constants and damping on all cloth springs.
                            // (Assuming that cloth particles are marked via clothID in the SoA.)
                            std::vector<SpringData>& springs = envSim->getSpringsReference();
                            ParticleSoA& soa = envSim->getSoAReference();
                            for (SpringData &sp : springs)
                            {
                                // Update only springs connecting cloth particles.
                                // (Assumes that if either particle has clothID >= 0, it is part of a cloth.)
                                if (soa.clothID[sp.p1Index] >= 0 || soa.clothID[sp.p2Index] >= 0)
                                {
                                    sp.springConstant = envSim->sharedParams.springConstant;
                                    sp.damping = envSim->sharedParams.dampingCoefficient;
                                }
                            }
            
                            // Finally, update the simulation so the new cloth is reflected.
                            blockWorld.updateSimulation(soa, springs);
                        }
                    }
                }
            }
        } // end event loop

        // Adjust relative mouse mode based on the active scene.
        if (activeScene == 1)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        else
        {
            if (!isPaused)
                SDL_SetRelativeMouseMode(SDL_TRUE);
            else
                SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        // Update simulation parameters (physics updates run asynchronously)
        if (activeScene == 1)
        {
            if (gui.isResetRequested())
            {
                scene1.simulation->stopAsyncUpdates();
                scene1.simulation->reset();
                // scene1.renderer.cameraReset(*scene1.simulation);
                gui.clearResetFlag();
                if (!isPaused)
                    scene1.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested())
            {
                scene1.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            scene1.simulation->setSpringConstant(gui.getSpringConstant());
            scene1.simulation->setDampingCoefficient(gui.getDampingCoefficient());
            OrbitCamera *orbitCam = dynamic_cast<OrbitCamera *>(scene1.camera);
            if (orbitCam)
            {
                orbitCam->setZoom(gui.getCameraZoom());
            }
        }
        else // activeScene == 2
        {
            if (gui.isResetRequested())
            {
                scene2.simulation->stopAsyncUpdates();
                scene2.simulation->reset();
                // reset env cloths here, function is EnvironmentSimulation::removeAllCloths();
            
                // Fully clear the external cubes.
                blockWorld.clearAllBlocks();
                blockWorld.updateSimulation(scene2.simulation->getSoAReference(),
                                            scene2.simulation->getSpringsReference());
            
                gui.clearResetFlag();
                if (!isPaused)
                    scene2.simulation->startAsyncUpdates();
            }
            if (gui.isDropStructureRequested())
            {
                scene2.simulation->dropStructure();
                gui.clearDropStructureFlag();
            }
            scene2.simulation->setSpringConstant(gui.getSpringConstant());
            scene2.simulation->setDampingCoefficient(gui.getDampingCoefficient());

            if (gui.isPlaceTemplateRequested()){
                // templates
                blockWorld.clearAllBlocks();
        
                if (gui.getSelectedTemplate() == 1) {
                    // Place Template 1 blocks relative to origin (or a base coordinate if desired)
                    blockWorld.placeHollowCube(template1);
                } else if (gui.getSelectedTemplate() == 2) {
                    // Place Template 2 blocks
                    for (const auto &coord : template2) {
                        blockWorld.addBlockForce(coord.x, coord.y, coord.z);
                    }
                }
                
                // Update the simulation's SoA with the new block positions.
                blockWorld.updateSimulation(scene2.simulation->getSoAReference(),
                                            scene2.simulation->getSpringsReference());
                
                // Clear the flag so we only place the template once per click.
                gui.clearPlaceTemplateFlag();
            }
        }

        // Prepare and draw ImGui content.
        gui.newFrame();
        gui.draw();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Measure Render Time ---
        double renderStartTime = getCurrentTime();

        if (activeScene == 1) {
            // Render Scene 1 normally.
            scene1.render();
        } else if (activeScene == 2) {
            FPSCamera* fpsCam = dynamic_cast<FPSCamera*>(scene2.camera);
            if (fpsCam) {
                // --- Compute light direction from GUI angles ---
                float phiDeg   = gui.getLightPhi();
                float thetaDeg = gui.getLightTheta();
                float phiRad   = glm::radians(phiDeg);
                float thetaRad = glm::radians(thetaDeg);
                glm::vec3 lightDir(
                    cos(phiRad) * cos(thetaRad),
                    sin(thetaRad),
                    sin(phiRad) * cos(thetaRad)
                );
                lightDir = glm::normalize(lightDir);
        
                // --- Render the directional shadow map ---
                scene2.renderer.renderDirectionalShadowMap(*scene2.simulation, lightDir);
        
                // --- Restore the main viewport (w and h are the window dimensions) ---
                glViewport(0, 0, w, h);
        
                // --- Render the scene into a cubemap using a shadow-aware renderer ---
                glm::vec3 camPos = fpsCam->getPosition();
                cubeCapture.renderToCubemap(*scene2.simulation, *fpsCam, camPos, scene2.renderer, lightDir);
        
                // --- Convert the cubemap to an equirectangular texture ---
                unsigned int equirectID = equiConverter.convert(cubeCapture.getCubemapID());
        
                // --- Finally, perform the distortion pass on the final texture ---
                distortionPass.setShowCrosshair(gui.getShowCrosshair());
                glm::mat4 viewMat4 = fpsCam->getViewMatrix();
                distortionPass.render(equirectID, gui.getFivePointFactor(), viewMat4);
            }
        }

        double renderEndTime = getCurrentTime();
        float renderFrameTimeMs = (float)((renderEndTime - renderStartTime) * 1000.0f);

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
        float avgFrameTime = std::accumulate(frameTimeQueue.begin(), frameTimeQueue.end(), 0.0f) / frameTimeQueue.size();
        float avgFPS = (avgFrameTime > 0.0f) ? (1000.0f / avgFrameTime) : 0.0f;

        // --- Update performance metrics in the GUI ---
        if (activeScene == 1)
        {
            int numParticles = (int)scene1.simulation->getSoA().position.size();
            int numSprings = (int)scene1.simulation->getSpringCount();
            double physicsStepTime = scene1.simulation->getLastPhysicsUpdateTime(); // in ms, assumed
            int effectiveSteps = scene1.simulation->getEffectiveStepsPerSecond();

            gui.setPerformanceMetrics((float)physicsStepTime,
                                      avgRenderTime,
                                      avgFrameTime,
                                      avgFPS,
                                      numParticles,
                                      numSprings,
                                      effectiveSteps);
        }
        else
        {
            int numParticles = (int)scene2.simulation->getSoA().position.size();
            int numSprings = (int)scene2.simulation->getSpringCount();
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

        if (activeScene == 2 && !isPaused)
        {
            // We are in the Environment scene, which uses the FPSCamera
            FPSCamera *fpsCam = dynamic_cast<FPSCamera *>(scene2.camera);
            if (fpsCam)
            {
                // Let the Movement system move the camera:
                Movement::updateFPSCamera(*fpsCam, (float)targetFrameDuration);
            }
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