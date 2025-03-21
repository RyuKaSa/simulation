#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;              // your camera proj * view
uniform mat4 uModel;            // local transform
uniform mat4 uLightSpaceMatrix; // for shadows

out vec4 FragPosLightSpace;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    FragPosLightSpace = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}