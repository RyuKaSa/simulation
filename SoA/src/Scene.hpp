#pragma once

#include "Simulation.hpp"  // now contains SimulationBase, ClothSimulation, etc.
#include "Renderer.hpp"

struct Scene {
    SimulationBase* simulation;  // pointer to SimulationBase (or derived)
    Renderer        renderer;
    bool            isInitialized;
    bool            isActive;

    // Constructor takes a pointer to SimulationBase
    Scene(SimulationBase* sim)
        : simulation(sim)
        , renderer(*simulation)  // pass the reference to the renderer constructor
        , isInitialized(false)
        , isActive(false)
    {
    }

    void init() {
        if (!isInitialized) {
            simulation->Initialization();
            isInitialized = true;
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
    }

    void shutdown() {
        simulation->stopAsyncUpdates();
    }
};