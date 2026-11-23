#version 450

// Text takes its coverage from the Michroma atlas; lines are anti-aliased from their distance to
// the centre line, interpolated across the quad.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vLine;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 7) uniform sampler2D textAtlas;

void main() {
    float coverage;
    if (vLine.x > 0.5) {
        coverage = clamp(vLine.y + 0.5 - abs(vLine.z), 0.0, 1.0);
    } else {
        coverage = texture(textAtlas, vUV).r;
    }
    outColor = vec4(vColor.rgb, vColor.a * coverage);
}
