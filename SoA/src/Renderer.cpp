#include <glad/glad.h>
#include <SDL_opengl.h>
#include "Renderer.hpp"

// Maximum supported instances
const size_t Renderer::maxInstances;

Renderer::Renderer(const SimulationBase &simulation) : camera(nullptr)
{
    if (!ballShader.load("src/shaders/ball.vs.glsl", "src/shaders/ball.fs.glsl"))
    {
        std::cerr << "Failed to load ball shaders." << std::endl;
    }
    if (!cubeShader.load("src/shaders/cube.vs.glsl", "src/shaders/cube.fs.glsl"))
    {
        std::cerr << "Failed to load cube shaders." << std::endl;
    }
    if (!springShader.load("src/shaders/spring.vs.glsl", "src/shaders/spring.fs.glsl"))
    {
        std::cerr << "Failed to load spring shaders." << std::endl;
    }
    if (!meshShader.load("src/shaders/mesh.vs.glsl", "src/shaders/mesh.fs.glsl"))
    {
        std::cerr << "Failed to load mesh shaders." << std::endl;
    }
    if (!gridShader.load("src/shaders/grid.vs.glsl", "src/shaders/grid.fs.glsl"))
    {
        std::cerr << "Failed to load grid shaders." << std::endl;
    }
    if (!depthShader.load("src/shaders/dirShadowDepth.vs.glsl", "src/shaders/dirShadowDepth.fs.glsl"))
    {
        std::cerr << "Failed to load dirShadowDepth shaders." << std::endl;
    }

    initDirectionalShadowMap();

    postProcessQuad = new FullscreenQuad();

    initBallGeometry();
    // initGrid();
    initCube();
    initSprings();
    initHexTriangles();
    // initTripleGrid(simulation);

    // print status of simulation
    std::cout << "Simulation has " << simulation.getSoA().position.size() << " particles.\n";
}

void Renderer::render(const SimulationBase &simulation)
{
    if (!camera)
    {
        std::cerr << "[Renderer] No camera set!\n";
        return;
    }

    // Begin off-screen rendering (if you are using the FBO functionality)
    camera->beginRender();

    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Retrieve view and projection matrices from our camera.
    float aspect = 16.0f / 9.0f; // Replace with your actual window aspect ratio.
    glm::mat4 proj = camera->getProjectionMatrix(aspect);
    glm::mat4 view = camera->getViewMatrix();

    // Use ball shader as an example:
    ballShader.use();
    ballShader.setUniform("uModel", glm::mat4(1.0f));
    ballShader.setUniform("uMVP", proj * view);

    renderGrid(proj, view);
    renderSprings(simulation, proj, view);
    // renderHexTriangles(simulation, proj, view) and renderBalls(simulation, proj, view)
    renderExternalCubes(simulation, proj, view);

    camera->endRender();

    postProcessQuad->render(camera->getRenderTexture());
}

void Renderer::renderWithMatrices(const SimulationBase &simulation, const glm::mat4 &view, const glm::mat4 &projection)
{
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Example: Use ball shader
    ballShader.use();
    ballShader.setUniform("uModel", glm::mat4(1.0f));
    ballShader.setUniform("uMVP", projection * view);

    // Render scene elements using the provided matrices.
    renderGrid(projection, view);
    renderSprings(simulation, projection, view);
    // Optionally, uncomment if needed:
    // renderHexTriangles(simulation, projection, view);
    // renderBalls(simulation, projection, view);
    renderExternalCubes(simulation, projection, view);
}

void Renderer::renderWithMatricesAndShadows(const SimulationBase &simulation,
                                            const glm::mat4 &view,
                                            const glm::mat4 &projection,
                                            const glm::vec3 &lightDir)
{
    // Set to filled mode and clear the framebuffer.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Render grid using gridShader (unchanged)
    gridShader.use();
    gridShader.setUniform("uModel", glm::mat4(1.0f));
    gridShader.setUniform("uMVP", projection * view);
    renderGrid(projection, view);

    // Render springs using springShader (unchanged)
    springShader.use();
    {
        glm::mat4 model(1.0f);
        glm::mat4 mvp = projection * view * model;
        springShader.setUniform("uMVP", mvp);
        springShader.setUniform("uColor", glm::vec3(0.25f, 0.39f, 0.59f));
    }
    // renderSprings(simulation, projection, view);
    // renderHexTriangles(simulation, projection, view);

    // Render cubes with instancing (for both EXTERNAL and BACKGROUND types).
    cubeShader.use();
    // Pass the shadow-related uniforms.
    cubeShader.setUniform("uLightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dirShadowTex);
    cubeShader.setUniform("uShadowMap", 1);
    // Also pass the light direction.
    cubeShader.setUniform("uLightDir", lightDir);

    // Gather instance data for cubes of type EXTERNAL and BACKGROUND.
    const ParticleSoA soa = simulation.getSoACopy();
    std::vector<glm::mat4> instanceModels;
    std::vector<glm::vec3> instanceColors;
    instanceModels.reserve(soa.position.size());
    instanceColors.reserve(soa.position.size());

    for (size_t i = 0; i < soa.position.size(); i++)
    {
        if (soa.type[i] == ParticleType::EXTERNAL || soa.type[i] == ParticleType::BACKGROUND)
        {
            // Build model matrix from position and scale.
            glm::vec3 pos(
                static_cast<float>(soa.position[i].x),
                static_cast<float>(soa.position[i].y),
                static_cast<float>(soa.position[i].z));
            glm::vec3 scl(
                static_cast<float>(soa.dimensions[i].x),
                static_cast<float>(soa.dimensions[i].y),
                static_cast<float>(soa.dimensions[i].z));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, scl);
            instanceModels.push_back(model);

            // Retrieve the cube's color.
            glm::vec3 color(
                static_cast<float>(soa.color[i].x),
                static_cast<float>(soa.color[i].y),
                static_cast<float>(soa.color[i].z));
            instanceColors.push_back(color);
        }
    }

    if (instanceModels.empty())
        return;

    size_t instanceCount = instanceModels.size();

    // Upload instance model matrices.
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(glm::mat4), instanceModels.data(), GL_DYNAMIC_DRAW);

    // Upload instance colors.
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(glm::vec3), instanceColors.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Compute and set the combined view-projection matrix.
    glm::mat4 mvp = projection * view;
    cubeShader.setUniform("uMVP", mvp);

    // Bind the cube VAO (which now has instanced attributes) and draw all instances.
    glBindVertexArray(cubeVAO);
    glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(instanceCount));
    glBindVertexArray(0);

    for (const ClothMesh &mesh : simulation.clothMeshes)
    {
        // 1. Rebuild mesh data using the current particle positions.
        std::vector<float> newMeshData = simulation.buildClothMeshData(mesh.startIndex, mesh.gridSize, mesh.nLayers);
    
        // 2. Update the existing VBO with the new data.
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, newMeshData.size() * sizeof(float), newMeshData.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    
        // 3. Set up the mesh shader and its uniforms.
        meshShader.use();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
        // Define the model transformation (here, identity matrix if no extra transform is needed).
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 mvp = projection * view * model;
        meshShader.setUniform("uMVP", mvp);
        meshShader.setUniform("uModel", model);
    
        // Set the shadow mapping uniforms. These values should be updated dynamically elsewhere.
        meshShader.setUniform("uLightSpaceMatrix", lightSpaceMatrix);
        meshShader.setUniform("uLightDir", lightDir);
    
        // Bind the shadow map texture to texture unit 1.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, dirShadowTex);
        meshShader.setUniform("uShadowMap", 1);
    
        // 4. Bind the VAO and draw the updated mesh.
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        glBindVertexArray(0);
    }
}

Renderer::~Renderer()
{
    glDeleteProgram(ballShader.getID());
    glDeleteProgram(cubeShader.getID());
    glDeleteProgram(springShader.getID());
    glDeleteProgram(meshShader.getID());
    glDeleteProgram(gridShader.getID());

    // Delete ball geometry
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &instancePosVBO);
    glDeleteBuffers(1, &instanceColorVBO);
    glDeleteBuffers(1, &instanceScaleVBO);

    // Delete grid
    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

    // Delete springs
    glDeleteVertexArrays(1, &springVAO);
    glDeleteBuffers(1, &springVBO);

    // Delete hex triangles
    glDeleteVertexArrays(1, &hexTriVAO);
    glDeleteBuffers(1, &hexTriVBO);

    // Delete cube geometry
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    delete postProcessQuad;
}

void Renderer::initBallGeometry()
{
    float radius = 0.1f;
    float vertices[(numSegments + 2) * 3];
    vertices[0] = 0.0f;
    vertices[1] = 0.0f;
    vertices[2] = 0.0f;
    for (int i = 0; i <= numSegments; i++)
    {
        float angle = i * 2.0f * 3.1415926f / numSegments;
        vertices[(i + 1) * 3 + 0] = radius * cos(angle);
        vertices[(i + 1) * 3 + 1] = radius * sin(angle);
        vertices[(i + 1) * 3 + 2] = 0.0f;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &instancePosVBO);
    glGenBuffers(1, &instanceColorVBO);
    glGenBuffers(1, &instanceScaleVBO);

    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void Renderer::initCube()
{
    // 24 vertices: 6 faces, 4 vertices per face.
    // Each vertex has 6 floats: 3 for position and 3 for normal.
    float cubeVertices[] = {
        // Front face
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        // Back face
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        // Left face
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        // Right face
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
        // Top face
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        // Bottom face
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f};

    // 36 indices for 12 triangles
    unsigned int cubeIndices[] = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Top face
        16, 17, 18, 18, 19, 16,
        // Bottom face
        20, 21, 22, 22, 23, 20};

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    // Vertex buffer & index buffer
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Vertex attribute for positions (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // Vertex attribute for normals (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // -------- Set up instancing --------

    // Instance model matrix (mat4 -> 4 vec4 attributes at locations 2,3,4,5)
    glGenBuffers(1, &cubeInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    for (unsigned int i = 0; i < 4; i++)
    {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(2 + i, 1); // Tell OpenGL this attribute advances per instance
    }

    // Instance color (vec3 at location = 6)
    glGenBuffers(1, &cubeInstanceColorVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
}

void Renderer::initSprings()
{
    glGenVertexArrays(1, &springVAO);
    glGenBuffers(1, &springVBO);
}

void Renderer::initHexTriangles()
{
    glGenVertexArrays(1, &hexTriVAO);
    glGenBuffers(1, &hexTriVBO);
    glBindVertexArray(hexTriVAO);

    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);
    // We don’t upload any data yet—just allocate 0 or an initial size.
    // (We’ll do actual bufferData later in render)

    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // -- Layout: We’ll store each vertex as 6 floats (Position + Normal) --
    //    position = loc 0, normal = loc 1

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Renderer::initHorizontalGrid(float gridExtent, float spacing)
{
    // Create a horizontal grid on the XZ plane at y = 0.
    // The grid will span from -gridExtent to +gridExtent in both X and Z directions.
    std::vector<float> gridVertices;

    // Create vertical lines (constant x, varying z)
    for (float x = -gridExtent; x <= gridExtent; x += spacing)
    {
        // Line from (x, 0, -gridExtent) to (x, 0, gridExtent)
        gridVertices.push_back(x);
        gridVertices.push_back(-0.5f);
        gridVertices.push_back(-gridExtent);

        gridVertices.push_back(x);
        gridVertices.push_back(-0.5f);
        gridVertices.push_back(gridExtent);
    }

    // Create horizontal lines (constant z, varying x)
    for (float z = -gridExtent; z <= gridExtent; z += spacing)
    {
        // Line from (-gridExtent, 0, z) to (gridExtent, 0, z)
        gridVertices.push_back(-gridExtent);
        gridVertices.push_back(-0.5f);
        gridVertices.push_back(z);

        gridVertices.push_back(gridExtent);
        gridVertices.push_back(-0.5f);
        gridVertices.push_back(z);
    }

    gridVertexCount = static_cast<int>(gridVertices.size() / 3);
    std::cout << "Initialized horizontal grid with vertex count: " << gridVertexCount << std::endl;

    // Generate VAO and VBO, and upload the grid data.
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::renderExternalCubes(const SimulationBase &simulation,
                                   const glm::mat4 &projection,
                                   const glm::mat4 &view)
{
    // Gather instance data for cubes of type EXTERNAL.
    const ParticleSoA soa = simulation.getSoACopy();
    std::vector<glm::mat4> instanceModels;
    std::vector<glm::vec3> instanceColors;
    instanceModels.reserve(soa.position.size());
    instanceColors.reserve(soa.position.size());

    for (size_t i = 0; i < soa.position.size(); i++)
    {
        if (soa.type[i] == ParticleType::EXTERNAL)
        {
            // Build model matrix from position and scale.
            glm::vec3 pos(
                static_cast<float>(soa.position[i].x),
                static_cast<float>(soa.position[i].y),
                static_cast<float>(soa.position[i].z));
            glm::vec3 scl(
                static_cast<float>(soa.dimensions[i].x),
                static_cast<float>(soa.dimensions[i].y),
                static_cast<float>(soa.dimensions[i].z));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, scl);
            instanceModels.push_back(model);

            // Retrieve the cube's color.
            glm::vec3 color(
                static_cast<float>(soa.color[i].x),
                static_cast<float>(soa.color[i].y),
                static_cast<float>(soa.color[i].z));
            instanceColors.push_back(color);
        }
    }

    if (instanceModels.empty())
        return;

    size_t instanceCount = instanceModels.size();

    // Set polygon mode to fill.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    cubeShader.use();

    // Set default shadow and lighting uniforms.
    cubeShader.setUniform("uLightSpaceMatrix", glm::mat4(0.0f));
    cubeShader.setUniform("uLightDir", glm::vec3(-0.2f, -0.9f, -0.45f));

    // Bind a default texture for the shadow map.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    cubeShader.setUniform("uShadowMap", 1);

    // Upload instance model matrices.
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(glm::mat4), instanceModels.data(), GL_DYNAMIC_DRAW);

    // Upload instance colors.
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceColorVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceCount * sizeof(glm::vec3), instanceColors.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Compute and set the combined view-projection matrix.
    glm::mat4 mvp = projection * view;
    cubeShader.setUniform("uMVP", mvp);

    // Bind the cube VAO (which is now configured for instancing) and draw all instances.
    glBindVertexArray(cubeVAO);
    glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(instanceCount));
    glBindVertexArray(0);
}

void Renderer::initTripleGrid(const SimulationBase &simulation)
{
    // 1) Compute bounding box by skipping every 3 positions
    const ParticleSoA soa = simulation.getSoACopy();
    if (soa.position.empty())
    {
        // If no particles, just do nothing
        gridVertexCount = 0;
        std::cerr << "No particles to create grid for!" << std::endl;
        return;
    }

    glm::dvec3 minPos(DBL_MAX), maxPos(-DBL_MAX);
    // We'll also ensure we read the first and last elements
    // so that we definitely include the extremes
    minPos = glm::min(minPos, soa.position.front());
    maxPos = glm::max(maxPos, soa.position.front());
    minPos = glm::min(minPos, soa.position.back());
    maxPos = glm::max(maxPos, soa.position.back());

    // skip ~ every 3rd to quickly get bounding box
    for (size_t i = 0; i < soa.position.size(); i += 3)
    {
        minPos = glm::min(minPos, soa.position[i]);
        maxPos = glm::max(maxPos, soa.position[i]);
    }

    // Expand the bounding box a bit so the planes are "just outside"
    double margin = 2.0 * glm::length(maxPos - minPos); // 10% margin
    if (margin < 0.5)
        margin = 0.5; // minimal margin
    glm::dvec3 expand(margin, margin, margin);
    minPos -= expand;
    maxPos += expand;

    // For clarity, convert to floats now
    float minX = (float)minPos.x;
    float minY = (float)minPos.y;
    float minZ = (float)minPos.z;
    float maxX = (float)maxPos.x;
    float maxY = (float)maxPos.y;
    float maxZ = (float)maxPos.z;

    // 2) Decide the plane positions
    // We'll do XY-plane at z = minZ
    // XZ-plane at y = minY
    // YZ-plane at x = minX

    // We'll define a spacing. You can choose how dense you want it
    float spacing = 0.5f; // or pick your own step
    // (You can also base spacing on the bounding box size if you want.)

    std::vector<float> gridVertices;
    gridVertices.reserve(1000);

    // Helper lambda to build line pairs
    auto addLine = [&](float x1, float y1, float z1,
                       float x2, float y2, float z2)
    {
        gridVertices.push_back(x1);
        gridVertices.push_back(y1);
        gridVertices.push_back(z1);

        gridVertices.push_back(x2);
        gridVertices.push_back(y2);
        gridVertices.push_back(z2);
    };

    // ============ PLANE 1: XY-plane at z = minZ ============
    // We'll draw lines parallel to X and parallel to Y
    for (float x = minX; x <= maxX; x += spacing)
    {
        addLine(x, minY, minZ, x, maxY, minZ);
    }
    for (float y = minY; y <= maxY; y += spacing)
    {
        addLine(minX, y, minZ, maxX, y, minZ);
    }

    // ============ PLANE 2: XZ-plane at y = minY ============
    for (float x = minX; x <= maxX; x += spacing)
    {
        addLine(x, minY, minZ, x, minY, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += spacing)
    {
        addLine(minX, minY, z, maxX, minY, z);
    }

    // ============ PLANE 3: YZ-plane at x = minX ============
    for (float y = minY; y <= maxY; y += spacing)
    {
        addLine(minX, y, minZ, minX, y, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += spacing)
    {
        addLine(minX, minY, z, minX, maxY, z);
    }

    // Now we know how many vertices we have
    gridVertexCount = (int)(gridVertices.size() / 3);
    std::cout << "Grid vertex count: " << gridVertexCount << std::endl;

    // 3) Create and upload to the VBO/VAO
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 gridVertices.size() * sizeof(float),
                 gridVertices.data(),
                 GL_STATIC_DRAW);

    // Simple position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Renderer::renderBalls(const SimulationBase &simulation,
                           const glm::mat4 &projection,
                           const glm::mat4 &view)
{
    // --- Use the ball shader first! ---
    ballShader.use();

    const ParticleSoA &soa = simulation.getSoA();
    std::vector<glm::vec3> positions, colors, scales;
    positions.reserve(soa.position.size());
    colors.reserve(soa.color.size());
    scales.reserve(soa.dimensions.size());

    // Filter out EXTERNAL particles so we only see the 'balls'
    for (size_t i = 0; i < soa.position.size(); i++)
    {
        if (soa.type[i] != ParticleType::EXTERNAL)
        {
            positions.emplace_back((float)soa.position[i].x,
                                   (float)soa.position[i].y,
                                   (float)soa.position[i].z);
            colors.emplace_back((float)soa.color[i].x,
                                (float)soa.color[i].y,
                                (float)soa.color[i].z);
            scales.emplace_back((float)soa.dimensions[i].x,
                                (float)soa.dimensions[i].y,
                                (float)soa.dimensions[i].z);
        }
    }

    // Clamp instance count if needed
    size_t instanceCount = positions.size();
    if (instanceCount > maxInstances)
    {
        instanceCount = maxInstances;
    }

    // -- Upload instance data (positions, colors, scales) to the VBOs -- //
    glBindBuffer(GL_ARRAY_BUFFER, instancePosVBO);
    void *posPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                    instanceCount * sizeof(glm::vec3),
                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (posPtr)
    {
        memcpy(posPtr, positions.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        // Fallback if mapping fails
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount * sizeof(glm::vec3),
                        positions.data());
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceColorVBO);
    void *colorPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                      instanceCount * sizeof(glm::vec3),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (colorPtr)
    {
        memcpy(colorPtr, colors.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount * sizeof(glm::vec3),
                        colors.data());
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceScaleVBO);
    void *scalePtr = glMapBufferRange(GL_ARRAY_BUFFER, 0,
                                      instanceCount * sizeof(glm::vec3),
                                      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (scalePtr)
    {
        memcpy(scalePtr, scales.data(), instanceCount * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        instanceCount * sizeof(glm::vec3),
                        scales.data());
    }

    // Set uniforms
    ballShader.setUniform("useInstance", 1);

    glm::mat4 model(1.0f);
    ballShader.setUniform("uModel", model);
    glm::mat4 mvp = projection * view * model;
    ballShader.setUniform("uMVP", mvp);

    // Draw as instanced triangles
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, numSegments + 2, (GLsizei)instanceCount);
    glBindVertexArray(0);
}

void Renderer::renderSprings(const SimulationBase &simulation,
                             const glm::mat4 &projection,
                             const glm::mat4 &view)
{
    const ParticleSoA &soa = simulation.getSoA();
    const std::vector<SpringData> &springs = simulation.getSprings();

    std::vector<glm::vec3> springVertices;
    springVertices.reserve(springs.size() * 2);

    for (const SpringData &spring : springs)
    {
        glm::vec3 p1 = glm::vec3(
            (float)soa.position[spring.p1Index].x,
            (float)soa.position[spring.p1Index].y,
            (float)soa.position[spring.p1Index].z);
        glm::vec3 p2 = glm::vec3(
            (float)soa.position[spring.p2Index].x,
            (float)soa.position[spring.p2Index].y,
            (float)soa.position[spring.p2Index].z);
        springVertices.push_back(p1);
        springVertices.push_back(p2);
    }

    springShader.use();
    glm::mat4 model(1.0f);
    glm::mat4 mvp = projection * view * model;
    springShader.setUniform("uMVP", mvp);
    springShader.setUniform("uColor", glm::vec3(0.25f, 0.39f, 0.59f));

    // Upload the vertex data
    glBindVertexArray(springVAO);
    glBindBuffer(GL_ARRAY_BUFFER, springVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 springVertices.size() * sizeof(glm::vec3),
                 springVertices.data(),
                 GL_DYNAMIC_DRAW);

    // Setup the vertex attrib again
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINES, 0, (GLsizei)springVertices.size());
    glBindVertexArray(0);
}

void Renderer::renderHexTriangles(const SimulationBase &simulation,
                                  const glm::mat4 &projection,
                                  const glm::mat4 &view)
{
    const std::vector<HexTriangle> &tris = simulation.getHexTriangles();
    if (tris.empty())
        return;

    // Build an array of floats for positions + normals
    // Each triangle has 3 vertices, each vertex = 6 floats (pos + normal).
    // So total floats = number of triangles * 3 * 6
    size_t vertexCount = tris.size() * 3;
    size_t floatCount = vertexCount * 6;
    std::vector<float> bufferData;
    bufferData.reserve(floatCount);

    for (auto &t : tris)
    {
        // For each triangle, push back (pos.x, pos.y, pos.z, normal.x, normal.y, normal.z)
        const glm::vec3 &n = t.normal;
        for (int i = 0; i < 3; i++)
        {
            const glm::vec3 &p = t.vertices[i];
            // position
            bufferData.push_back(p.x);
            bufferData.push_back(p.y);
            bufferData.push_back(p.z);
            // normal
            bufferData.push_back(n.x);
            bufferData.push_back(n.y);
            bufferData.push_back(n.z);
        }
    }

    // Now upload to the VBO
    glBindVertexArray(hexTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hexTriVBO);

    // Upload all the vertex+normal data
    glBufferData(GL_ARRAY_BUFFER,
                 bufferData.size() * sizeof(float),
                 bufferData.data(),
                 GL_DYNAMIC_DRAW);

    meshShader.use();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    // Suppose we want model=identity for debug
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;
    meshShader.setUniform("uMVP", mvp);

    meshShader.setUniform("uColor", glm::vec3(1.0f));

    // Draw
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);

    glBindVertexArray(0);
}

void Renderer::renderClothMesh(const SimulationBase &simulation,
                               const glm::mat4 &projection,
                               const glm::mat4 &view,
                               const ClothMesh &clothMesh)
{
    // 1. Rebuild mesh data using the current particle positions.
    std::vector<float> newMeshData = simulation.buildClothMeshData(clothMesh.startIndex, clothMesh.gridSize, clothMesh.nLayers);

    // 2. Update the existing VBO with the new data.
    glBindBuffer(GL_ARRAY_BUFFER, clothMesh.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, newMeshData.size() * sizeof(float), newMeshData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 3. Set up the mesh shader and its uniforms.
    meshShader.use();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Define the model transformation (identity if the mesh is already in world space)
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;
    meshShader.setUniform("uMVP", mvp);
    meshShader.setUniform("uModel", model);

    // Set the shadow mapping uniforms:
    meshShader.setUniform("uLightSpaceMatrix", lightSpaceMatrix);
    meshShader.setUniform("uLightDir", glm::vec3(-0.2f, -0.9f, -0.45f));

    // Bind the shadow map texture to texture unit 1.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dirShadowTex);
    meshShader.setUniform("uShadowMap", 1);

    // 4. Bind the VAO and draw the updated mesh.
    glBindVertexArray(clothMesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, clothMesh.vertexCount);
    glBindVertexArray(0);
}

void Renderer::renderGrid(const glm::mat4 &projection, const glm::mat4 &view)
{
    glm::mat4 model(1.0f);
    glm::mat4 mvp = projection * view * model;

    gridShader.use();
    gridShader.setUniform("uModel", model);
    gridShader.setUniform("uMVP", mvp);
    gridShader.setUniform("uColor", glm::vec3(0.3f, 0.3f, 0.3f));
    // std::cout << "Rendering grid with vertex count: " << gridVertexCount << std::endl;
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void Renderer::initDirectionalShadowMap()
{
    glGenFramebuffers(1, &dirShadowFBO);

    glGenTextures(1, &dirShadowTex);
    glBindTexture(GL_TEXTURE_2D, dirShadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_SIZE, SHADOW_SIZE, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // clamp to border so that outside of [0,1] is in light
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, white);

    glBindFramebuffer(GL_FRAMEBUFFER, dirShadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, dirShadowTex, 0);
    glDrawBuffer(GL_NONE); // no color
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Shadow FBO not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    shadowsInitialized = true;
}

void Renderer::renderDirectionalShadowMap(const SimulationBase &simulation, const glm::vec3 &lightDir)
{
    if (!shadowsInitialized)
        return;

    // 1) We'll define an orthographic box big enough for your environment.
    float orthoSize = 15.0f; // tune to your scene
    float nearPlane = -10.0f;
    float farPlane = 50.0f;
    // We'll just pick "center" at (0,0,0). If your environment is large, compute a bounding box from the environment.
    glm::vec3 center(0.0f, 0.0f, 0.0f);

    // position the light far away in the -lightDir direction
    glm::vec3 lightPos = center - 30.0f * lightDir;
    // pick an up vector not collinear with lightDir
    glm::vec3 up(0, 1, 0);
    if (fabs(glm::dot(up, lightDir)) > 0.9f)
        up = glm::vec3(1, 0, 0);

    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize,
                                     -orthoSize, orthoSize,
                                     nearPlane, farPlane);
    glm::mat4 lightView = glm::lookAt(lightPos, center, up);
    lightSpaceMatrix = lightProj * lightView;

    // 2) Render to the shadow map FBO
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, dirShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    depthShader.use();
    depthShader.setUniform("uLightSpaceMatrix", lightSpaceMatrix);

    // Draw your environment geometry with depthShader
    // Example: draw external cubes as simple boxes
    const ParticleSoA soa = simulation.getSoACopy();
    for (size_t i = 0; i < soa.position.size(); i++)
    {
        if (soa.type[i] == ParticleType::EXTERNAL)
        {
            glm::vec3 pos((float)soa.position[i].x,
                          (float)soa.position[i].y,
                          (float)soa.position[i].z);
            glm::vec3 scl((float)soa.dimensions[i].x,
                          (float)soa.dimensions[i].y,
                          (float)soa.dimensions[i].z);
            glm::mat4 model(1.0f);
            model = glm::translate(model, pos);
            model = glm::scale(model, scl);

            depthShader.setUniform("uModel", model);

            glBindVertexArray(cubeVAO);
            // e.g. glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }

    // After drawing cubes in the shadow pass:
    for (const ClothMesh &mesh : simulation.clothMeshes)
    {
        // Optionally rebuild or update the mesh data if needed
        // (just like in your main render pass). Then bind the VAO:
        glBindVertexArray(mesh.vao);

        // Set the model matrix. If your cloth doesn’t have an extra transform,
        // you can use an identity matrix or any offset/rotation you need.
        glm::mat4 model = glm::mat4(1.0f);
        depthShader.setUniform("uModel", model);

        // Now draw it:
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // restore main viewport (handled in your main loop)
}

void Renderer::renderSceneWithShadows(const SimulationBase &simulation,
                                      const glm::mat4 &view,
                                      const glm::mat4 &projection,
                                      const glm::vec3 &lightDir)
{
    // Ensure filled mode.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    cubeShader.use();

    // Set the light-space matrix (for shadow mapping).
    cubeShader.setUniform("uLightSpaceMatrix", lightSpaceMatrix);

    // Bind the shadow texture to texture unit 1.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dirShadowTex);
    cubeShader.setUniform("uShadowMap", 1);

    // *** Set the light direction uniform here ***
    cubeShader.setUniform("uLightDir", lightDir);

    // Render external cubes.
    const ParticleSoA soa = simulation.getSoACopy();
    for (size_t i = 0; i < soa.position.size(); i++)
    {
        if (soa.type[i] == ParticleType::EXTERNAL)
        {
            glm::vec3 pos(
                (float)soa.position[i].x,
                (float)soa.position[i].y,
                (float)soa.position[i].z);
            glm::vec3 scl(
                (float)soa.dimensions[i].x,
                (float)soa.dimensions[i].y,
                (float)soa.dimensions[i].z);

            // Build model matrix.
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos);
            model = glm::scale(model, scl);

            cubeShader.setUniform("uModel", model);
            glm::mat4 mvp = projection * view * model;
            cubeShader.setUniform("uMVP", mvp);

            // Optionally, set the color uniform using the particle color.
            glm::vec3 color(
                (float)soa.color[i].x,
                (float)soa.color[i].y,
                (float)soa.color[i].z);
            cubeShader.setUniform("uColor", color);

            glBindVertexArray(cubeVAO);
            // Use 36 indices (triangles) for filled cube rendering.
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}

// void Renderer::adjustCameraToFit(const SimulationBase& simulation)
// {
//     std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
//     if (positions.empty()) return;

//     size_t n = positions.size();
//     glm::dvec3 minPos = positions[0];
//     glm::dvec3 maxPos = positions[0];

//     for (size_t i = 0; i < n; i += 3) {
//         minPos = glm::min(minPos, positions[i]);
//         maxPos = glm::max(maxPos, positions[i]);
//     }
//     if ((n - 1) % 3 != 0) {
//         minPos = glm::min(minPos, positions.back());
//         maxPos = glm::max(maxPos, positions.back());
//     }
//     glm::dvec3 center = (minPos + maxPos)*0.5;
//     double maxExtent   = glm::length(maxPos - minPos);
//     float newDistance  = (float)glm::clamp(maxExtent*0.8, (double)minCameraDistance, (double)maxCameraDistance);
//     targetDistance     = glm::mix(targetDistance, newDistance, lerpFactor);

//     targetCenter = glm::vec3((float)center.x, (float)center.y, (float)center.z);
//     targetPosition = glm::vec3((float)maxPos.x,
//                                (float)(maxPos.y + 0.3),
//                                (float)(center.z + targetDistance));
// }

// void Renderer::cameraReset(const SimulationBase& simulation)
// {
//     std::vector<glm::dvec3> positions = simulation.getStructureParticlePositions();
//     if (positions.empty()) return;

//     glm::dvec3 minPos = positions[0];
//     glm::dvec3 maxPos = positions[0];
//     glm::dvec3 sum(0.0);

//     for (const auto& pos : positions) {
//         minPos = glm::min(minPos, pos);
//         maxPos = glm::max(maxPos, pos);
//         sum += pos;
//     }
//     glm::dvec3 center = sum / (double)positions.size();
//     double extent = glm::length(maxPos - minPos);
//     extent = glm::min(extent, (double)maxCameraDistance);
//     float desiredDistance = (float)glm::max(extent*1.1, (double)minCameraDistance);
//     if (glm::length(center) > (double)maxCameraDistance) {
//         center = glm::normalize(center)*(double)maxCameraDistance;
//     }
//     cameraTarget = glm::vec3((float)center.x,
//                              (float)center.y,
//                              (float)center.z);
//     targetCenter = cameraTarget;
//     targetDistance = desiredDistance;
//     targetPosition = cameraTarget;
//     cameraPosition = targetPosition;
// }