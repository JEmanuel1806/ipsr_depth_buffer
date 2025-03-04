#version 440 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D normalTex; 

void main()
{
    vec3 normal = texture(normalTex, TexCoords).rgb;
    FragColor = vec4(normal * 0.5 + 0.5, 1.0);

}
