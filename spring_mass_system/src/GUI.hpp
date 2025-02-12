#ifndef GUI_H
#define GUI_H

#include <SDL.h>
#include <SDL_opengl.h>

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
private:
    float springConstant;
    float dampingCoefficient;
    int physicsSteps;
};

#endif // GUI_H