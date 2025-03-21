#include "GUI.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

GUI::GUI(SDL_Window* window, SDL_GLContext glContext)
    : springConstant(3000.0f),
      dampingCoefficient(70.0f),
      physicsSteps(1000),
      reset(false),
      dropStructureRequested(false),
      performancePhysicsStepTime(0.0f),
      performanceRenderFrameTime(0.0f),
      performanceTotalFrameTime(0.0f),
      performanceFPS(0.0f),
      performanceNumParticles(0),
      performanceNumSprings(0),
      performancePhysicsStepsPerSecond(0),
      gridSize(50),            // default grid size parameter
      springRestLength(1.0f),  // default spring rest length multiplier
      gravityStrength(9.81f),  // default gravity magnitude
      particleMass(5.0f),      // default particle mass
      cameraZoom(0.5f),
      fivePointFactor(0.0f),
      showCrosshair(true),
      lightPhi(45.0f),
      lightTheta(45.0f)
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
    // --- Simulation Control Window ---
    ImGui::Begin("Simulation Controls");

    // Top-level action buttons
    if (ImGui::Button("Reset")) { 
        reset = true; 
    }
    ImGui::SameLine();
    if (ImGui::Button("Drop Structure")) { 
        dropStructureRequested = true; 
    }
    // --- Camera Settings ---
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Camera Zoom", &cameraZoom, 0.1f, 3.0f);
        ImGui::SliderFloat("Five-Point Factor", &fivePointFactor, 0.0f, 1.0f);
        ImGui::Checkbox("Show Crosshair", &showCrosshair);
        ImGui::SliderFloat("Light Phi", &lightPhi,   0.0f, 360.0f);
        ImGui::SliderFloat("Light Theta", &lightTheta, -80.0f, 80.0f);
    }
    ImGui::Separator();

    // Physics settings
    if (ImGui::CollapsingHeader("Physics Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Spring Constant", &springConstant, 10.0f, 20000.0f);
        ImGui::SliderFloat("Damping Coefficient", &dampingCoefficient, 1.0f, 200.0f);
    }

    // Grid and structure parameters
    if (ImGui::CollapsingHeader("Grid & Structure", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Grid Size", &gridSize, 10, 500);
        ImGui::SliderFloat("Spring Rest Length", &springRestLength, 0.5f, 2.0f);
        ImGui::SliderFloat("Gravity Strength", &gravityStrength, 0.0f, 20.0f);
        ImGui::SliderFloat("Particle Mass", &particleMass, 1.0f, 100.0f);
    }

    ImGui::End();

    // --- Performance Metrics Window ---
    ImGui::Begin("Performance Metrics");
    ImGui::Text("Physics Step Time: %.3f ms", performancePhysicsStepTime * 1000.0f);
    ImGui::Text("Render Frame Time: %.3f ms", performanceRenderFrameTime);
    ImGui::Text("Total Frame Time: %.3f ms", performanceTotalFrameTime);
    ImGui::Text("FPS: %.1f", performanceFPS);
    ImGui::Separator();
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

float GUI::getSpringConstant() const { return springConstant; }
float GUI::getDampingCoefficient() const { return dampingCoefficient; }
int   GUI::getPhysicsSteps() const { return physicsSteps; }
bool  GUI::isResetRequested() { return reset; }
bool  GUI::isDropStructureRequested() const { return dropStructureRequested; }

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

void GUI::clearDropStructureFlag() {
    dropStructureRequested = false;
}