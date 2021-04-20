#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexcoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D image;

void main() {
    outColor = texture(image, fragTexcoord);
}
