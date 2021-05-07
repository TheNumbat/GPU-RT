
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT vec3 payload;
layout(location = 1) rayPayloadEXT bool shadowed;

hitAttributeEXT vec3 attribs;

layout(binding = 1, scalar) readonly buffer SceneDescs {
	Scene_Obj objects[];
};

layout(binding = 2, scalar) readonly buffer Vertices { 
	Vertex v[]; 
} vertices[];

layout(binding = 3) readonly buffer Indices {
	uint i[];
} indices[];

layout(binding = 4) uniform sampler2D Textures[];

layout(binding = 5) uniform accelerationStructureEXT topLevelAS;

void main() {

	uint objId = objects[gl_InstanceCustomIndexEXT].index;

	ivec3 ind = ivec3(indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 0],
					  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1],
					  indices[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);

	Vertex v0 = vertices[nonuniformEXT(objId)].v[ind.x];
	Vertex v1 = vertices[nonuniformEXT(objId)].v[ind.y];
	Vertex v2 = vertices[nonuniformEXT(objId)].v[ind.z];

	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

	vec3 normal = v0.norm_ty.xyz * barycentrics.x + v1.norm_ty.xyz * barycentrics.y + v2.norm_ty.xyz * barycentrics.z;
	normal = normalize(vec3(objects[gl_InstanceCustomIndexEXT].modelIT * vec4(normal, 0.0)));

	vec3 worldPos = v0.pos_tx.xyz * barycentrics.x + v1.pos_tx.xyz * barycentrics.y + v2.pos_tx.xyz * barycentrics.z;
	worldPos = vec3(objects[gl_InstanceCustomIndexEXT].model * vec4(worldPos, 1.0));


	vec3 L;
	float lightIntensity = consts.lightIntensity;
	float lightDistance  = 100000.0;
	if(consts.lightType == 0) {
		vec3 lDir      = consts.lightPosition - worldPos;
		lightDistance  = length(lDir);
		lightIntensity = consts.lightIntensity / (lightDistance * lightDistance);
		L              = normalize(lDir);
	} else {
		L = normalize(consts.lightPosition - vec3(0));
	}
	float dotNL = dot(normal, L);
	shadowed = true;
	if(dotNL > 0) {
		float tMin   = 0.001;
		float tMax   = lightDistance;
		vec3  origin = worldPos;
		vec3  rayDir = L;
		uint  flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		traceRayEXT(topLevelAS,  // acceleration structure
				flags,       // rayFlags
				0xFF,        // cullMask
				0,           // sbtRecordOffset
				0,           // sbtRecordStride
				1,           // missIndex
				origin,      // ray origin
				tMin,        // ray min range
				rayDir,      // ray direction
				tMax,        // ray max range
				1            // payload (location = 1)
		);
	}


	int texIdx = objects[gl_InstanceCustomIndexEXT].albedo_tex;
	vec3 albedo = objects[gl_InstanceCustomIndexEXT].albedo.xyz;
	if(texIdx >= 0) {
		vec2 tc0 = vec2(v0.pos_tx.w, v0.norm_ty.w);
		vec2 tc1 = vec2(v1.pos_tx.w, v1.norm_ty.w);
		vec2 tc2 = vec2(v2.pos_tx.w, v2.norm_ty.w);
		vec2 texCoord = tc0 * barycentrics.x + tc1 * barycentrics.y + tc2 * barycentrics.z;
		albedo = texture(Textures[texIdx], texCoord).xyz;
	}

	if(shadowed) 
		payload = 0.05 * albedo;
	else
		payload = dotNL * albedo;
}
