#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

#include "Renderer.hpp"
#include "Simulation.hpp"
#include "GUI.hpp"

int main(int argc, char* argv[]) {
    // Initialize SDL and set OpenGL attributes for GLSL 330 Core
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Balls Simulation", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
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
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Create modules
    Renderer renderer;
    Simulation simulation;
    GUI gui(window, glContext);

    bool running = true;
    SDL_Event event;
    const float targetFrameTime = 1000.0f / 60.0f; // Fixed 60 fps

    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            gui.processEvent(event);
        }

        gui.newFrame();
        gui.draw(); // Draw GUI (slider for global parameter)

        // Pass global parameter to simulation and update (dt = 1/60)
        simulation.setSpringConstant(gui.getSpringConstant());
        simulation.update(1.0f / 60.0f);

        renderer.render(simulation);  // Render balls (from simulation)
        gui.render();  // Render the GUI on top

        SDL_GL_SwapWindow(window);

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < targetFrameTime)
            SDL_Delay(targetFrameTime - frameTime);
    }

    gui.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}