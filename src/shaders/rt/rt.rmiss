
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT Ray_Payload payload;

void main() {
    payload.hit = false;
}
