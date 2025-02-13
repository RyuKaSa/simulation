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
    void clearResetFlag();
private:
    float springConstant;
    float dampingCoefficient;
    int physicsSteps;
    bool reset;
};

#endif // GUI_H