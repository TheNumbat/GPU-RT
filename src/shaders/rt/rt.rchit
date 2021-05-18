
#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rtcommon.glsl"

layout(location = 0) rayPayloadInEXT CH_Payload payload;

hitAttributeEXT vec3 attribs;

layout(binding = 1, std430) readonly buffer SceneDescs {
	Scene_Obj objects[];
};

layout(binding = 2, std430) readonly buffer SceneLights {
	Scene_Light lights[];
};

layout(binding = 3, std430) readonly buffer Vertices { 
	Vertex v[]; 
} vertices[];

layout(binding = 4, std430) readonly buffer Indices {
	uint i[];
} indices[];

layout(binding = 5) uniform sampler2D Textures[];

layout(binding = 6) uniform accelerationStructureEXT TLAS;

MatInfo mat_info(HitInfo hit) {

	MatInfo mat;
	
	int albedoIdx = objects[gl_InstanceCustomIndexEXT].albedo_tex;
	mat.albedo = objects[gl_InstanceCustomIndexEXT].albedo.xyz;
	if(albedoIdx >= 0) {
		mat.albedo = texture(Textures[albedoIdx], hit.texcoord).xyz;
	}

	int emissiveIdx = objects[gl_InstanceCustomIndexEXT].emissive_tex;
	mat.emissive = objects[gl_InstanceCustomIndexEXT].emissive.xyz;
	if(emissiveIdx >= 0) {
		mat.emissive = texture(Textures[emissiveIdx], hit.texcoord).xyz;
	}

	int mrIdx = objects[gl_InstanceCustomIndexEXT].metal_rough_tex;
	mat.roughness = objects[gl_InstanceCustomIndexEXT].metal_rough.y;
	if(mrIdx >= 0) {
		mat.roughness = texture(Textures[mrIdx], hit.texcoord).y;
	}

	int nIdx = objects[gl_InstanceCustomIndexEXT].normal_tex;
	mat.use_tanspace = nIdx >= 0;
	if(mat.use_tanspace) {
		mat.tanspaceNormal = texture(Textures[nIdx], hit.texcoord).xyz * 2.0 - 1.0;
	}
	return mat;
}

HitInfo hit_info() {

	uint objId = objects[gl_InstanceCustomIndexEXT].index;

	HitInfo hit;
	ivec3 ind = ivec3(indices[objId].i[3 * gl_PrimitiveID + 0],
					  indices[objId].i[3 * gl_PrimitiveID + 1],
					  indices[objId].i[3 * gl_PrimitiveID + 2]);

	Vertex v0 = vertices[objId].v[ind.x];
	Vertex v1 = vertices[objId].v[ind.y];
	Vertex v2 = vertices[objId].v[ind.z];

	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

	hit.normal = v0.norm_ty.xyz * barycentrics.x + v1.norm_ty.xyz * barycentrics.y + v2.norm_ty.xyz * barycentrics.z;
	hit.normal = normalize(vec3(objects[gl_InstanceCustomIndexEXT].modelIT * vec4(hit.normal, 0.0)));

	vec3 t0 = v0.tangent.xyz * v0.tangent.w;
	vec3 t1 = v1.tangent.xyz * v1.tangent.w;
	vec3 t2 = v2.tangent.xyz * v2.tangent.w;
	hit.tangent = t0 * barycentrics.x + t1 * barycentrics.y + t2 * barycentrics.z;
	hit.tangent = normalize(vec3(objects[gl_InstanceCustomIndexEXT].modelIT * vec4(hit.tangent, 0.0)));

	hit.pos = v0.pos_tx.xyz * barycentrics.x + v1.pos_tx.xyz * barycentrics.y + v2.pos_tx.xyz * barycentrics.z;
	hit.pos = vec3(objects[gl_InstanceCustomIndexEXT].model * vec4(hit.pos, 1.0));

	vec2 tc0 = vec2(v0.pos_tx.w, v0.norm_ty.w);
	vec2 tc1 = vec2(v1.pos_tx.w, v1.norm_ty.w);
	vec2 tc2 = vec2(v2.pos_tx.w, v2.norm_ty.w);
	hit.texcoord = tc0 * barycentrics.x + tc1 * barycentrics.y + tc2 * barycentrics.z;
	return hit;
}

ShadeInfo shade_info(HitInfo hit, MatInfo mat) {

	ShadeInfo shade;

	shade.wo = gl_WorldRayDirectionEXT;
	if(dot(shade.wo, hit.normal) > 0) hit.normal = -hit.normal;

	shade.T = hit.tangent;
	shade.N = hit.normal;

	if(mat.use_tanspace && consts.use_normal_map == 1) {
		shade.B = cross(shade.N, shade.T);
		shade.N = normalize(mat3(shade.T, shade.B, shade.N) * mat.tanspaceNormal);
	}

	normalCoords(shade.N, shade.T, shade.B);
	return shade;
}

vec3 light_sample(vec3 p) {
	
	uint l_idx = randu(payload.seed, 0, consts.n_lights);
	uint o_idx = lights[l_idx].index;
	uint n_tris = lights[l_idx].n_triangles;
	uint t_idx = randu(payload.seed, 0, n_tris);

	ivec3 ind = ivec3(indices[o_idx].i[3 * t_idx + 0],
					  indices[o_idx].i[3 * t_idx + 1],
					  indices[o_idx].i[3 * t_idx + 2]);

	Vertex v0 = vertices[o_idx].v[ind.x];
	Vertex v1 = vertices[o_idx].v[ind.y];
	Vertex v2 = vertices[o_idx].v[ind.z];

	vec3 bary = sampleTriangle(payload.seed);
	vec3 point = v0.pos_tx.xyz * bary.x + v1.pos_tx.xyz * bary.y + v2.pos_tx.xyz * bary.z;
	point = vec3(objects[o_idx].model * vec4(point, 1.0));

	return normalize(point - p);
}

float light_pdf(vec3 p, vec3 d) {

	float oacc = 0;
	for(uint l = 0; l < consts.n_lights; l++) {
		
		float tacc = 0;
		uint o_idx = lights[l].index;
		uint n_tris = lights[l].n_triangles;

		for(uint t = 0; t < n_tris; t++) {

			ivec3 ind = ivec3(indices[o_idx].i[3 * t + 0],
							  indices[o_idx].i[3 * t + 1],
							  indices[o_idx].i[3 * t + 2]);

			vec3 v0 = vertices[o_idx].v[ind.x].pos_tx.xyz;
			vec3 v1 = vertices[o_idx].v[ind.y].pos_tx.xyz;
			vec3 v2 = vertices[o_idx].v[ind.z].pos_tx.xyz;

			v0 = vec3(objects[o_idx].model * vec4(v0, 1.0));
			v1 = vec3(objects[o_idx].model * vec4(v1, 1.0));
			v2 = vec3(objects[o_idx].model * vec4(v2, 1.0));

			tacc += trianglePDF(p, d, v0, v1, v2);
		}

		oacc += tacc / float(n_tris);
	}

	return oacc / float(consts.n_lights);
}

void integrate_mis(HitInfo hit, MatInfo mat, ShadeInfo shade) {

	payload.emissive = payload.misWeight * mat.emissive;
	if(payload.is_direct) return;

	payload.nextO = hit.pos;

	if(any(greaterThan(mat.emissive, vec3(0)))) {
		payload.depth = consts.max_depth;
		return;
	}
	
	if(mat.roughness == 0) {

		payload.nextD = reflect(shade.wo, shade.N);
		payload.nextWeight = mat.albedo;
		payload.misWeight = 1;

	} else {
		
		vec3 wi_light = light_sample(hit.pos);
		float light_pdf_l = light_pdf(hit.pos, wi_light);

		if(light_pdf_l != 0) {
			float light_pdf_m = GGX_pdf(mat, shade, wi_light);
			vec3 light_atten = GGX_eval(mat, shade, wi_light);
			payload.shadowWeight = light_atten / light_pdf_l * powerHeuristic(light_pdf_l, light_pdf_m);
			payload.shadowD = wi_light;
		}

		vec3 wi_brdf;
		if(!GGX_sample(payload.seed, mat, shade, wi_brdf)) {
			payload.depth = consts.max_depth;
			return;
		}

		float brdf_pdf_m = GGX_pdf(mat, shade, wi_brdf);

		if(brdf_pdf_m != 0) {
			float brdf_pdf_l = light_pdf(hit.pos, wi_brdf);
			vec3 brdf_atten = GGX_eval(mat, shade, wi_brdf);
			payload.nextWeight = brdf_atten / brdf_pdf_m;
			payload.misWeight = powerHeuristic(brdf_pdf_m, brdf_pdf_l);
		} else {
			payload.depth = consts.max_depth;
			return;
		}

		payload.nextD = wi_brdf;
	}
}

void integrate_mats(HitInfo hit, MatInfo mat, ShadeInfo shade) {

	payload.nextO = hit.pos;
	payload.emissive = mat.emissive;

	if(any(greaterThan(mat.emissive, vec3(0)))) {
		payload.depth = consts.max_depth;
		return;
	}
	
	if(mat.roughness == 0) {
		
		payload.nextD = reflect(shade.wo, shade.N);
		payload.nextWeight = mat.albedo;

	} else {

		vec3 wi;
		if(!GGX_sample(payload.seed, mat, shade, wi)) {
			payload.depth = consts.max_depth;
			return;
		}
		
		float pdf = GGX_pdf(mat, shade, wi);
		vec3 atten = GGX_eval(mat, shade, wi);
		if(pdf != 0) {
			payload.nextWeight = atten / pdf;
		} else {
			payload.depth = consts.max_depth;
			return;
		}
	
		payload.nextD = wi;
	}
}

void main() {

	HitInfo hit = hit_info();
	MatInfo mat = mat_info(hit);
	ShadeInfo shade = shade_info(hit, mat);

	if(consts.use_nee == 1 && consts.n_lights > 0) {
		integrate_mis(hit, mat, shade);
	} else {
		integrate_mats(hit, mat, shade);
	}
}
