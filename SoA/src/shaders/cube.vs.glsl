#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;  // New normal attribute

uniform mat4 uMVP;              // Camera projection * view
uniform mat4 uModel;            // Local transform
uniform mat4 uLightSpaceMatrix; // For shadow mapping

out vec4 FragPosLightSpace;
out vec3 FragNormal;            // Pass normal to fragment shader

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    FragPosLightSpace = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
    // Transform the normal properly (especially important if non-uniform scaling is used)
    FragNormal = mat3(transpose(inverse(uModel))) * aNormal;
}