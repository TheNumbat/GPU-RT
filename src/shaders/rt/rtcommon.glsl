
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

struct Scene_Obj {
	mat4 model;
	mat4 modelIT;
	uint index;
	uint pad[3];
};

struct Vertex {
	vec4 pos_tx;
	vec4 norm_ty;
};

layout(push_constant) uniform Constants 
{
    vec4 clearColor;
    vec3 lightPosition;
    float lightIntensity;
    int lightType;
};

