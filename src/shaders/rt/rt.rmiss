
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT CH_Payload payload;

void main() {
    
    if(payload.depth == 0)
        payload.emissive = consts.clearColor.xyz;
    else
        payload.emissive = consts.envlight.xyz;

    payload.depth = consts.max_depth;
}
