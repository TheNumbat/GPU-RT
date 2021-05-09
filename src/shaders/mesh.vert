#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNorm;
layout(location = 2) in vec4 inTangent;

layout(location = 0) out vec3 fragNorm;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out vec3 fragTangent;
layout(location = 3) out vec2 fragTexcoord;

layout(push_constant) uniform constants
{
	mat4 M;
} pushes;

layout(binding = 0) uniform UniformBufferObject {
    mat4 V, P, iV, iP;
} ubo;

void main() {
    gl_Position = ubo.P * ubo.V * pushes.M * vec4(inPosition.xyz, 1.0);
    fragNorm = inNorm.xyz;
    fragPos = inPosition.xyz;
    fragTangent = inTangent.xyz;
    fragTexcoord = vec2(inPosition.w, inNorm.w);
}
