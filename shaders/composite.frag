#version 450

// Lays the volume, marched at reduced resolution, over the full-resolution sky. The blend does
// the work: what is behind is dimmed by the transmittance and the volume's light added.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 5) uniform sampler2D volumeLight;

void main() {
    outColor = textureLod(volumeLight, vUV, 0.0);
}
