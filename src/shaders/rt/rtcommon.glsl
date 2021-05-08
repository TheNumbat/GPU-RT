
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define M_PI 3.141592
#define LARGE_DIST 1000000000.0

struct Scene_Obj {
	mat4 model;
	mat4 modelIT;
	vec4 albedo;
	vec4 emissive;
	vec4 metal_rough;
	int albedo_tex;
	int emissive_tex;
	int metal_rough_tex;
	uint index;
};

struct Vertex {
	vec4 pos_tx;
	vec4 norm_ty;
};

struct hitPayload {
	vec3 emissive;
	vec3 nextO;
	vec3 nextD;
	vec3 nextWeight;
	uint seed;
	int depth;
};

layout(push_constant) uniform Constants 
{
	vec4 clearColor;
	vec4 envlight;
	int frame;
	int samples;
	int max_depth;
} consts;

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint val0, uint val1) {
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;
	for(uint n = 0; n < 16; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev) {
	uint LCG_A = 1664525u;
	uint LCG_C = 1013904223u;
	prev = (LCG_A * prev + LCG_C);
	return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float randf(inout uint prev) {
	return (float(lcg(prev)) / float(0x01000000));
}

vec3 cosineHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z) {
	float r1 = randf(seed);
	float r2 = randf(seed);
	float sq = sqrt(1.0 - r2);
	vec3 direction = vec3(cos(2 * M_PI * r1) * sq, sin(2 * M_PI * r1) * sq, sqrt(r2));
	direction = direction.x * x + direction.y * y + direction.z * z;
	return direction;
}

void normalCoords(in vec3 N, out vec3 Nt, out vec3 Nb) {
	if(abs(N.x) > abs(N.y))
		Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
	else
		Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
	Nb = cross(N, Nt);
}
