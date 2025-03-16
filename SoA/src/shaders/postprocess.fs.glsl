#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uTexture;

void main()
{
    // Simply output the texture color.
    vec3 color = texture(uTexture, TexCoords).rgb;
    FragColor = vec4(color, 1.0);
}