#include "GUI.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

GUI::GUI(SDL_Window* window, SDL_GLContext glContext)
    : springConstant(10000.0f),
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
      gridSize(50),  
      springRestLength(1.0f),   
      gravityStrength(9.81f),  
      particleMass(5.0f), 
      cameraZoom(0.5f),
      fivePointFactor(0.0f),
      showCrosshair(true),
      lightPhi(45.0f),
      lightTheta(-45.0f),
      selectedTemplate(0),         // start with "None"
      placeTemplateRequested(false),
      selectedStructure(0),
      nLayers(2),
      showClothMesh(false)
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

    // ——— Global: Reset (always) ———
    if (ImGui::Button("Reset")) {
        reset = true;
    }

    // ——— Scene-1 only: Drop Structure ———
    if (activeScene == Scene::Cloth) {
        ImGui::SameLine();
        if (ImGui::Button("Drop Structure")) {
            dropStructureRequested = true;
        }
    }

    // ——— Global: Physics Settings ———
    if (ImGui::CollapsingHeader("Physics Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Spring Constant", &springConstant, 10.0f, 20000.0f);
        ImGui::SliderFloat("Damping Coefficient", &dampingCoefficient, 1.0f, 200.0f);
        ImGui::SliderFloat("Particle Mass", &particleMass, 1.0f, 100.0f);
        ImGui::SliderFloat("Gravity Strength", &gravityStrength, 0.0f, 20.0f);
    }

    // ——— Global: Grid & Structure ———
    if (ImGui::CollapsingHeader("Grid & Structure", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Grid Size", &gridSize, 10, 500);
        ImGui::SliderFloat("Spring Rest Length", &springRestLength, 0.5f, 2.0f);
    }

    // ——— Scene-1: Cloth settings ———
    if (activeScene == Scene::Cloth) {
        if (ImGui::CollapsingHeader("Cloth Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Camera Zoom", &cameraZoom, 0.1f, 3.0f);
            const char* items[] = {
                "Multi-layer square grid",
                "Square grid",
                "Multi-layer hex grid",
                "Hex grid",
                "Spring Cord"
            };
            ImGui::Combo("Structure", &selectedStructure, items, IM_ARRAYSIZE(items));
            ImGui::SliderInt("Number of Layers", &nLayers, 1, 10);
            ImGui::Checkbox("Show Mesh (ONLY MULTI LAYER SQRE GRID)", &showClothMesh);
        }
    }
    // ——— Scene-2: Environment settings ———
    else { // Environment
        if (ImGui::CollapsingHeader("Environment Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Five-Point Factor", &fivePointFactor, 0.0f, 1.0f);
            ImGui::SliderFloat("Light Phi", &lightPhi,   0.0f, 360.0f);
            ImGui::SliderFloat("Light Theta", &lightTheta, -80.0f, 80.0f);

            ImGui::Separator();
            ImGui::Text("Select a Template:");
            ImGui::RadioButton("None",       &selectedTemplate, 0);
            ImGui::RadioButton("Template 1", &selectedTemplate, 1);
            ImGui::RadioButton("Template 2", &selectedTemplate, 2);
            if (ImGui::Button("Place Template")) {
                placeTemplateRequested = true;
            }
        }
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