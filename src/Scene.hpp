#pragma once

#include "Simulation.hpp"
#include "Renderer.hpp"
#include "Camera.hpp"

struct Scene {
    SimulationBase* simulation;
    Renderer        renderer;
    Camera*         camera;
    bool            isInitialized;
    bool            isActive;

    // Constructor takes a pointer to SimulationBase
    Scene(SimulationBase* sim, Camera* cam)
        : simulation(sim)
        , renderer(*simulation)  // pass the reference to the renderer constructor
        , camera(cam)
        , isInitialized(false)
        , isActive(false)
    {
        renderer.setCamera(camera);
    }

    void init() {
        if (!isInitialized) {
            simulation->Initialization();
            // renderer.initTripleGrid(*simulation);
            isInitialized = true;
        }
        if (OrbitCamera* orbitCam = dynamic_cast<OrbitCamera*>(camera)) {
            orbitCam->adjustToFit(*simulation); // Reset camera to fit the simulation.
        }
    }

    void resume() {
        isActive = true;
        simulation->startAsyncUpdates();
    }

    void pause() {
        isActive = false;
        simulation->stopAsyncUpdates();
    }

    void render() {
        renderer.render(*simulation);
        if (OrbitCamera* orbit = dynamic_cast<OrbitCamera*>(camera)) {
            orbit->adjustToFit(*simulation);
        }
    }

    void shutdown() {
        simulation->stopAsyncUpdates();
        if (camera) {
            delete camera;
            camera = nullptr;
        }
    }
};