#version 450

// Overlay quads, built on the CPU each frame and read here by vertex index.

struct Vertex {
    vec4 posUv;   // NDC position, texture coordinates
    vec4 color;   // straight alpha
    vec4 line;    // mode (0 text, 1 line), half width px, signed distance across px, unused
};

layout(set = 0, binding = 6, std430) readonly buffer Overlay {
    Vertex vertices[];
};

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vLine;

void main() {
    Vertex v = vertices[gl_VertexIndex];
    gl_Position = vec4(v.posUv.xy, 0.0, 1.0);
    vUV    = v.posUv.zw;
    vColor = v.color;
    vLine  = v.line.xyz;
}
