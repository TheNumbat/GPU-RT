
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 fragTexcoord;

vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0f, 1.0);
    fragTexcoord = pos / 2.0f + vec2(0.5f);
}
