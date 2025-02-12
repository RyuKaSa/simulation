# Physics Simulation Template

This repository serves as a starting point for developing 2D physics simulations using **C++**, **OpenGL (GLAD)**, **SDL2**, and **GLM**. It includes basic rendering, a simulation loop, and a structure to build upon.

## 📂 Project Structure
```
📁 src/                # Source code
    ├── Simulation.cpp # Core physics simulation logic
    ├── Renderer.cpp   # Handles OpenGL rendering
    ├── GUI.cpp        # UI (if applicable)
📁 external/           # Third-party libraries (GLAD, ImGui)
📁 glimac/             # Additional utilities (GLAD, shaders)
📁 build/              # Generated build files
📄 CMakeLists.txt      # Build configuration
```

## 🔧 How to Extend the Template
- Implement new physics behavior inside `Simulation.cpp`.
- Modify rendering logic in `Renderer.cpp` to customize visuals.
- Add UI elements in `GUI.cpp` (if applicable).

## 🤝 Contributing
Feel free to fork and improve this template. Open a pull request if you add something useful!

