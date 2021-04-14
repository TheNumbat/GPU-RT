#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexcoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexcoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 M, V, P;
} ubo;

void main() {
    gl_Position = ubo.P * ubo.V * ubo.M * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexcoord = inTexcoord;
}
