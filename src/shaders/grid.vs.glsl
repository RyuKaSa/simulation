#version 330 core

layout (location = 0) in vec3 aPos;   // Vertex position

uniform mat4 uMVP;  // The combined model-view-projection matrix
uniform mat4 uModel;
uniform vec3 uColor;

// We can pass color to the fragment stage
out vec3 fColor;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    fColor = uColor; // pass along color
}