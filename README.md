# BallsSimulation

A real-time physics & graphics demo written in C++ using OpenGL, SDL2, ImGui, and GLAD.  
It can simulate cloth, various structures, and handle external collisions & forces.

Check out the videos here : [videos](https://drive.google.com/drive/folders/1U1Nbbpa4fmdBlV-UMEvRtFU4o-qO40TP?usp=share_link)

---

## Features

- **Cloth simulation** (mass-spring model)  
- **Particle-based rigid structures**  
- External collisions (floor, spheres, meshes…)  
- Real-time forces (gravity, wind, user-defined)  
- Interactive debug UI via ImGui  

---

## Prerequisites

1. **C++17** compiler (GCC ≥ 7, Clang ≥ 5, MSVC ≥ 2017)  
2. [CMake](https://cmake.org/) ≥ 3.10  
3. **SDL2** development libraries (via pkg-config)  
4. **GLM** math library  
5. **GLAD** (included)  
6. **ImGui** (included)

On macOS (Homebrew), you can install SDL2 & glm with:

```bash
brew install sdl2 glm
```

On Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsdl2-dev libglm-dev
```

---

## Project Structure

```
.
├── CMakeLists.txt
├── external/
│   ├── imgui/            # ImGui sources 
├── glimac/
│   └── third-party/glad/ # GLAD loader
├── src/                  # The app code (simulation/rendering)
├── videos/               # Video recordings of the simulation
└── README.md             # This file
```

---

## Build & Run

From your project root:

```bash
# (Re)generate build directory and compile
rm -rf build
mkdir build && cd build
cmake ..                  # Configure
make -j8                  # Build
cd ..

# Launch the simulation
./build/BallsSimulation
```

> **Tip:** If the school’s lab PCs need special include or linker paths, let me know—I can adapt the CMake setup next week on site.

---

## Usage & Controls
 
- **ImGui panel**:  
  - Toggle simulation modes  
  - Adjust spring constants, damping, forces  
  - Spawn / remove objects  
- **Change Scene** (press `X`):  
  - Scene 1: Basic cloth simulation  
  - Scene 2: Advanced simulation with user controls
- **Pause Simulation** (press `C`):  
  - Pause or resume the simulation
  - In Scene 2, this also toggles the cursor visibility
- **Place Cloth** (press `V`):
  - Only in Scene 2, when pointing at a block

---

## Acknowledgments

- [GLAD](https://glad.dav1d.de/) — OpenGL loader  
- [SDL2](https://www.libsdl.org/) — windowing & input  
- [ImGui](https://github.com/ocornut/imgui) — debug & UI  