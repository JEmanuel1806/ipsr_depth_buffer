#version 440 core

uniform sampler2D depthTex; 
in vec2 texCoords;             
out vec4 FragColor;

void main()
{
    float depth = texture(depthTex, texCoords).r;
    float normalizedDepth = clamp(depth, 0.0, 1.0);
    vec3 color = mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), normalizedDepth);

    FragColor = vec4(color, 1.0);
}
