
// mostly based on https://github.com/lukedan/ReSTIR-Vulkan

#define RESERVOIR_SIZE 1

struct Reservoir {
    vec3 pos;
	vec3 normal;
    vec3 emissive;
	float w_sum;
    float w;
	uint n_seen;
};

struct ReSTIRConstants {
    mat4 prev_PV;
	uint new_samples;
	uint temporal_multiplier;
};

void res_update(inout uint seed, inout Reservoir res, float weight, vec3 pos, vec3 normal, vec3 emissive) {

	res.n_seen++;
    res.w_sum += weight;

	if(randf(seed) < weight / res.w_sum) {
		res.pos = pos;
		res.normal = normal;
		res.emissive = emissive;
	}
}

Reservoir res_new() {
	Reservoir result;
	result.w_sum = 0;
    result.w = 0;
	result.n_seen = 0;
	return result;
}

