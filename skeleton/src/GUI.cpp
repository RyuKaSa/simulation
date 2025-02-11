#include "GUI.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

GUI::GUI(SDL_Window* window, SDL_GLContext glContext) : springConstant(0.5f) {
    // Initialize ImGui context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
}

GUI::~GUI() {}

void GUI::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void GUI::draw() {
    ImGui::Begin("Physics Parameters");
    ImGui::SliderFloat("Spring Constant", &springConstant, 0.0f, 1.0f);
    ImGui::End();
}

void GUI::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::processEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void GUI::cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

float GUI::getSpringConstant() const {
    return springConstant;
}