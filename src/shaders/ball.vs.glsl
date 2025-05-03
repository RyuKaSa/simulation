#version 330 core


layout (location = 0) in vec3 aPos;
// Instance attributes
layout (location = 1) in vec3 offset;   // Ball center
layout (location = 2) in vec3 instColor;  // Ball color
layout (location = 3) in float scale;     // Ball scale

uniform mat4 uMVP;
uniform mat4 uModel;
uniform int useInstance;
uniform vec3 uColor;

out vec3 fColor;


void main()
{

    if (useInstance == 1) {
        vec4 posNonInstance = uModel * vec4(aPos, 1.0);
        vec4 posInstance = vec4(aPos * scale + offset, 1.0);
        vec4 pos = mix(posNonInstance, posInstance, float(useInstance));
        fColor = mix(uColor, instColor, float(useInstance));
        gl_Position = uMVP * pos;
    } else {
        vec4 pos = uModel * vec4(aPos, 1.0);
        fColor = uColor;
        gl_Position = uMVP * pos;
    }
    // vec4 posNonInstance = uModel * vec4(aPos, 1.0);
    // vec4 posInstance = vec4(aPos * scale + offset, 1.0);
    // vec4 pos = mix(posNonInstance, posInstance, float(useInstance));
    // fColor = mix(uColor, instColor, float(useInstance));
    // gl_Position = uMVP * pos;
}