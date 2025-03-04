#version 440 core

uniform sampler2D depthTex;
uniform vec2 iResolution;
in vec2 texCoords;
out vec4 FragColor;

uniform mat4 invProj;

vec3 getPos(ivec2 fragCoord, float depth) {
    vec2 ndc = (vec2(fragCoord) / iResolution) * 2.0 - 1.0;
    vec4 clipSpace = vec4(ndc, depth, 1.0);
    vec4 viewSpace = invProj * clipSpace;
    return viewSpace.xyz / viewSpace.w;
}

vec3 computeNormalNaive(const sampler2D depthTex, ivec2 p) {

    // clamp for outside boundary point (not below 0 and not above resolution)
    ivec2 left = clamp(p - ivec2(1, 0), ivec2(0), textureSize(depthTex, 0) - ivec2(1));
    ivec2 right = clamp(p + ivec2(1, 0), ivec2(0), textureSize(depthTex, 0) - ivec2(1));
    ivec2 top = clamp(p + ivec2(0, 1), ivec2(0), textureSize(depthTex, 0) - ivec2(1));
    ivec2 bottom = clamp(p - ivec2(0, 1), ivec2(0), textureSize(depthTex, 0) - ivec2(1));

    vec3 l1 = getPos(left, texelFetch(depthTex, left, 0).r);
    vec3 r1 = getPos(right, texelFetch(depthTex, right, 0).r);
    vec3 t1 = getPos(top, texelFetch(depthTex, top, 0).r);
    vec3 b1 = getPos(bottom, texelFetch(depthTex, bottom, 0).r);

    vec3 dpdx = r1 - l1;
    vec3 dpdy = t1 - b1;
    return normalize(cross(dpdx, dpdy));
}

void main() {
    ivec2 fragCoord = ivec2(gl_FragCoord.xy);
    vec3 normal = computeNormalNaive(depthTex, fragCoord);
    vec3 color = normal * 0.5 + 0.5; // to RGB 0 - 1 range
    FragColor = vec4(color, 1.0);
}