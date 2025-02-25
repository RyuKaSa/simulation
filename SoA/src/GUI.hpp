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
    int getPhysicsSteps() const;
    bool isResetRequested();
    bool isDropStructureRequested() const;

    void clearResetFlag();
    void clearDropStructureFlag();

    void setPerformanceMetrics(float physicsStepTime, float renderFrameTime,
                               float totalFrameTime, float fps,
                               int numParticles, int numSprings,
                               int physicsStepsPerSecond);
private:
    float springConstant;
    float dampingCoefficient;
    int physicsSteps;
    bool reset;
    bool dropStructureRequested;

    float performancePhysicsStepTime = 0.0f;
    float performanceRenderFrameTime = 0.0f;
    float performanceTotalFrameTime  = 0.0f;
    float performanceFPS             = 0.0f;
    int   performanceNumParticles    = 0;
    int   performanceNumSprings      = 0;
    int   performancePhysicsStepsPerSecond = 0;
};

#endif // GUI_H