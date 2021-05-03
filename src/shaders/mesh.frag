#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragNorm;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in vec2 fragTexcoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragPos, 1.0f);
}
