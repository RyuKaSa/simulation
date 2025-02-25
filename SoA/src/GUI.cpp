#include "GUI.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

GUI::GUI(SDL_Window* window, SDL_GLContext glContext)
    : springConstant(3000.0f),
      dampingCoefficient(70.0f),
      physicsSteps(1000),
      reset(false),
      dropStructureRequested(false)
{
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
    ImGui::SliderFloat("Spring Constant", &springConstant, 10.0f, 10000.0f);
    ImGui::SliderFloat("Damping Coefficient", &dampingCoefficient, 0.0f, 100.0f);
    if (ImGui::Button("Reset")) { reset = true; }
    if (ImGui::Button("Drop Structure")) { dropStructureRequested = true; }
    ImGui::End();

    ImGui::Begin("Performance Metrics");
    ImGui::Text("Physics Step Time: %.3f ms", performancePhysicsStepTime*1000.0f);
    ImGui::Text("Render Frame Time: %.3f ms", performanceRenderFrameTime*1000.0f);
    ImGui::Text("Total Frame Time: %.3f ms", performanceTotalFrameTime*1000.0f);
    ImGui::Text("FPS: %.1f", performanceFPS);
    ImGui::Text("Particles: %d", performanceNumParticles);
    ImGui::Text("Springs: %d", performanceNumSprings);
    ImGui::Text("Physics Steps/sec: %d", performancePhysicsStepsPerSecond);
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

float GUI::getDampingCoefficient() const {
    return dampingCoefficient;
}

int GUI::getPhysicsSteps() const {
    return physicsSteps;
}

bool GUI::isResetRequested() {
    return reset;
}

void GUI::clearResetFlag() {
    std::cout << "reset cleared" << std::endl;
    reset = false;
}

void GUI::setPerformanceMetrics(float physicsStepTime,
                                float renderFrameTime,
                                float totalFrameTime,
                                float fps,
                                int numParticles,
                                int numSprings,
                                int physicsStepsPerSecond)
{
    performancePhysicsStepTime = physicsStepTime;
    performanceRenderFrameTime = renderFrameTime;
    performanceTotalFrameTime  = totalFrameTime;
    performanceFPS             = fps;
    performanceNumParticles    = numParticles;
    performanceNumSprings      = numSprings;
    performancePhysicsStepsPerSecond = physicsStepsPerSecond;
}

bool GUI::isDropStructureRequested() const {
    return dropStructureRequested;
}

void GUI::clearDropStructureFlag() {
    dropStructureRequested = false;
}