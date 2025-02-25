#ifndef GUI_H
#define GUI_H

#include <SDL.h>
#include <SDL_opengl.h>
#include <iostream>

class GUI {
public:
    GUI(SDL_Window* window, SDL_GLContext glContext);
    ~GUI();
    void newFrame();
    void render();
    void draw();
    void processEvent(const SDL_Event& event);
    void cleanup();
    float getSpringConstant() const;
    float getDampingCoefficient() const;
    int   getPhysicsSteps() const;
    bool isResetRequested();
    bool isDropStructureRequested() const;

    void clearResetFlag();
    void clearDropStructureFlag();

    // Set performance metrics (existing)
    void setPerformanceMetrics(float physicsStepTime, float renderFrameTime,
                               float totalFrameTime, float fps,
                               int numParticles, int numSprings,
                               int physicsStepsPerSecond);

    // New getters so that the simulation code can pick up the new parameters on reset
    int   getGridSize() const { return gridSize; }
    float getSpringRestLength() const { return springRestLength; }
    float getGravityStrength() const { return gravityStrength; }
    float getParticleMass() const { return particleMass; }

private:
    float springConstant;
    float dampingCoefficient;
    int physicsSteps;
    bool reset;
    bool dropStructureRequested;

    // Performance metrics (existing)
    float performancePhysicsStepTime;
    float performanceRenderFrameTime;
    float performanceTotalFrameTime;
    float performanceFPS;
    int   performanceNumParticles;
    int   performanceNumSprings;
    int   performancePhysicsStepsPerSecond;

    // simulation parameters (affecting reset/creation)
    int   gridSize;            // e.g. number of cells in the grid
    float springRestLength;    // spring rest length multiplier
    float gravityStrength;     // gravity magnitude
    float particleMass;        // mass for new particles
};

#endif // GUI_H