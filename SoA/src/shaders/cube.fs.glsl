#version 330 core

in vec4 FragPosLightSpace;
in vec3 FragNormal;
in vec3 InstanceColor;  // Added instance color input

out vec4 FragColor;

uniform sampler2D uShadowMap;
uniform vec3 uLightDir;   // Light direction (normalized) pointing FROM the surface TO the light.

void main()
{
    // -------------------------------------------------------------
    // 1) Compute the projection coords for shadow map look-up
    // -------------------------------------------------------------
    // FragPosLightSpace is in light's clip space (from your VS). 
    // First do perspective divide, then map from [-1,1] to [0,1].
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // If the fragment is outside the [0,1] range, skip shadowing
    // (to avoid reading invalid tex coords). This is optional
    // if you already handle out-of-bounds in your pipeline.
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        // Out of shadow map range; treat as unshadowed
        // but still do your N·L
        float ndotl = max(dot(normalize(FragNormal), -uLightDir), 0.0);
        // We’ll do a very simple grey ambient (0.2) + direct (0.8 * ndotl).
        float litShade = 0.2 + 0.8 * ndotl;
        FragColor = vec4(InstanceColor * litShade, 1.0);
        return;
    }

    // -------------------------------------------------------------
    // 2) Compare current fragment depth with shadow map’s depth
    // -------------------------------------------------------------
    float currentDepth = projCoords.z;
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    
    // A small bias can help reduce shadow acne
    float bias = 0.0005;
    
    // Shadow factor: 0 = not in shadow, 1 = fully shadowed
    float shadow = (currentDepth > closestDepth + bias) ? 1.0 : 0.0;

    // -------------------------------------------------------------
    // 3) Compute N·L for simple diffuse lighting factor
    //    (We reverse the light direction to check if the normal
    //     is facing the direction from which the light arrives.)
    // -------------------------------------------------------------
    float ndotl = dot(normalize(FragNormal), -uLightDir);
    ndotl = clamp(ndotl, 0.0, 1.0);

    // -------------------------------------------------------------
    // 4) Decide how lit vs. unlit looks. You mentioned wanting
    //    a non-discrete fallback (greyer) when in shadow and
    //    a simple grey instead of pure black for unlit areas.
    // -------------------------------------------------------------
    // If fully lit:
    //    - Start with a minimal “ambient” grey (e.g. 0.2)
    //    - Add up to ~0.8 extra brightness if facing the light
    float litShade = 0.2 + 0.8 * ndotl;

    // If in shadow (or partially shadowed in a more advanced approach),
    // we scale it down, but do not go fully black. For instance:
    //    - 0.1 ambient plus 0.3 scaled by ndotl → range ~0.1..0.4
    float shadowShade = 0.1 + 0.3 * ndotl;

    // “shadow” is 0 or 1 in this simple example.  If you want softer
    // edges, you’d do PCF or an average of multiple samples.
    float finalShade = mix(litShade, shadowShade, shadow);

    FragColor = vec4(InstanceColor * finalShade, 1.0);
}