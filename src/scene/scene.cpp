
#include <sstream>
#include "scene.h"

#define TINYGLTF_NOEXCEPTION
#include <sf_libs/tiny_gltf.h>

Scene::Scene(unsigned int start) : next_id(start), first_id(start) {
}

unsigned int Scene::used_ids() {
	return next_id;
}

unsigned int Scene::reserve_id() {
	return next_id++;
}

unsigned int Scene::add(Object&& obj) {
	assert(objs.find(obj.id()) == objs.end());
	objs.emplace(std::make_pair(obj.id(), std::move(obj)));
	return obj.id();
}

void Scene::erase(unsigned int id) {
	objs.erase(id);
}

size_t Scene::size() const {
	return objs.size();
}

void Scene::clear() {
	objs.clear();
	textures.clear();
}

bool Scene::empty() {
	return objs.size() == 0;
}

Object& Scene::get(unsigned int id) {
	auto entry = objs.find(id);
	assert(entry != objs.end());
	return entry->second;
}

void Scene::parse_mesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh, Pose pose) {

	using namespace tinygltf;
	for (const auto &meshPrimitive : gltfMesh.primitives) {
		
		std::vector<Vec3> positions, normals;
		std::vector<Vec4> tangents;
		std::vector<Vec2> texcoords;
		std::vector<VK::Mesh::Index> indices;

		// Boolean used to check if we have converted the vertex buffer format
		bool convertedToTriangleList = false;
		{
			const auto &indicesAccessor = model.accessors[meshPrimitive.indices];
			const auto &bufferView = model.bufferViews[indicesAccessor.bufferView];
			const auto &buffer = model.buffers[bufferView.buffer];
			const auto dataAddress = buffer.data.data() + bufferView.byteOffset +
									indicesAccessor.byteOffset;
			const auto byteStride = indicesAccessor.ByteStride(bufferView);
			const auto count = indicesAccessor.count;

			// Allocate the index array in the pointer-to-base declared in the
			// parent scope
			switch (indicesAccessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_BYTE:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(char*)(dataAddress + byteStride * i));
				}
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(unsigned char*)(dataAddress + byteStride * i));
				}
				break;

			case TINYGLTF_COMPONENT_TYPE_SHORT:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(short*)(dataAddress + byteStride * i));
				}
				break;

			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(unsigned short*)(dataAddress + byteStride * i));
				}
				break;

			case TINYGLTF_COMPONENT_TYPE_INT:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(int*)(dataAddress + byteStride * i));
				}
				break;

			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				for(size_t i = 0; i < count; i++) {
					indices.push_back(*(unsigned int*)(dataAddress + byteStride * i));
				}
				break;
			default:
				break;
			}
		}

		switch (meshPrimitive.mode) {
			// We re-arrange the indices so that it describe a simple list of
			// triangles
			case TINYGLTF_MODE_TRIANGLE_FAN:
			if (!convertedToTriangleList) {
				// This only has to be done once per primitive
				convertedToTriangleList = true;

				// We steal the guts of the vector
				auto triangleFan = std::move(indices);
				indices.clear();

				// Push back the indices that describe just one triangle one by one
				for (size_t i{2}; i < triangleFan.size(); ++i) {
					indices.push_back(triangleFan[0]);
					indices.push_back(triangleFan[i - 1]);
					indices.push_back(triangleFan[i]);
				}
			}
			case TINYGLTF_MODE_TRIANGLE_STRIP:
			if (!convertedToTriangleList) {
				// This only has to be done once per primitive
				convertedToTriangleList = true;

				auto triangleStrip = std::move(indices);
				indices.clear();

				for (size_t i{2}; i < triangleStrip.size(); ++i) {
				indices.push_back(triangleStrip[i - 2]);
				indices.push_back(triangleStrip[i - 1]);
				indices.push_back(triangleStrip[i]);
				}
			}
			case TINYGLTF_MODE_TRIANGLES:  // this is the simpliest case to handle
			{

			for (const auto &attribute : meshPrimitive.attributes) {
				const auto attribAccessor = model.accessors[attribute.second];
				const auto &bufferView =
					model.bufferViews[attribAccessor.bufferView];
				const auto &buffer = model.buffers[bufferView.buffer];
				const auto dataPtr = buffer.data.data() + bufferView.byteOffset +
									attribAccessor.byteOffset;
				const auto byte_stride = attribAccessor.ByteStride(bufferView);
				const auto count = attribAccessor.count;

				if (attribute.first == "POSITION") {
					switch (attribAccessor.type) {
						case TINYGLTF_TYPE_VEC3: {
						switch (attribAccessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
							for (size_t i = 0; i < count; i++) {
								Vec3* v = (Vec3*)(dataPtr + i * byte_stride);
								positions.push_back(*v);
							}
						}
						break;
						case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
							switch (attribAccessor.type) {
							case TINYGLTF_TYPE_VEC3: {
								for (size_t i = 0; i < count; i++) {
									double* values = (double*)(dataPtr + i * byte_stride);
									Vec3 p{(float)values[0], (float)values[1], (float)values[2]};
									positions.push_back(p);
								}
							} break;
							default:
								break;
							}
							break;
							default:
							break;
						}
						} break;
					}
				}

				if (attribute.first == "TANGENT") {
					switch (attribAccessor.type) {
						case TINYGLTF_TYPE_VEC4: {
						switch (attribAccessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_FLOAT:
							for (size_t i = 0; i < count; i++) {
								Vec4* v = (Vec4*)(dataPtr + i * byte_stride);
								tangents.push_back(*v);
							}
						}
						break;
						case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
							switch (attribAccessor.type) {
							case TINYGLTF_TYPE_VEC4: {
								for (size_t i = 0; i < count; i++) {
									double* values = (double*)(dataPtr + i * byte_stride);
									Vec4 p{(float)values[0], (float)values[1], (float)values[2], (float)values[3]};
									tangents.push_back(p);
								}
							} break;
							default:
								break;
							}
							break;
							default:
							break;
						}
						} break;
					}
				}

				if (attribute.first == "NORMAL") {
					switch (attribAccessor.type) {
						case TINYGLTF_TYPE_VEC3: {
						switch (attribAccessor.componentType) {
							case TINYGLTF_COMPONENT_TYPE_FLOAT: {
							for(size_t i = 0; i < count; i++) {
								Vec3* n = (Vec3*)(dataPtr + i * byte_stride);
								normals.push_back(*n);
							}
							} break;
							case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
							for(size_t i = 0; i < count; i++) {
								double* values = (double*)(dataPtr + i * byte_stride);
								Vec3 n{(float)values[0], (float)values[1], (float)values[2]};
								normals.push_back(n);
							}
							} break;
							default:
							std::cerr << "Unhandeled componant type for normal\n";
						}
						} break;
						default:
						std::cerr << "Unhandeled vector type for normal\n";
					}
				}

				// Face varying comment on the normals is also true for the UVs
				if (attribute.first == "TEXCOORD_0") {
					switch (attribAccessor.type) {
					case TINYGLTF_TYPE_VEC2: {
						switch (attribAccessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_FLOAT: {
							for(size_t i = 0; i < count; i++) {
								Vec2* n = (Vec2*)(dataPtr + i * byte_stride);
								texcoords.push_back(*n);
							}
						} break;
						case TINYGLTF_COMPONENT_TYPE_DOUBLE: {
							for(size_t i = 0; i < count; i++) {
								double* values = (double*)(dataPtr + i * byte_stride);
								Vec2 tc{(float)values[0], (float)values[1]};
								texcoords.push_back(tc);
							}
						} break;
						default:
							std::cerr << "unrecognized vector type for UV";
						}
					} break;
					default:
						std::cerr << "unreconized componant type for UV";
					}
				}
			}
			break;

			default:
				std::cerr << "primitive mode not implemented";
				break;

			// These aren't triangles:
			case TINYGLTF_MODE_POINTS:
			case TINYGLTF_MODE_LINE:
			case TINYGLTF_MODE_LINE_LOOP:
				std::cerr << "primitive is not triangle based, ignoring";
			}
		}

		::Material mat;

		const tinygltf::Material& glmat = model.materials[meshPrimitive.material];

		const auto& basecolorfactor = glmat.pbrMetallicRoughness.baseColorFactor;
		const auto emissivefactor = glmat.emissiveFactor;

		mat.albedo = Vec3{(float)basecolorfactor[0], (float)basecolorfactor[1], (float)basecolorfactor[2]};
		mat.albedo_tex = glmat.pbrMetallicRoughness.baseColorTexture.index;

		mat.emissive = Vec3{(float)emissivefactor[0], (float)emissivefactor[1], (float)emissivefactor[2]};
		mat.emissive_tex = glmat.emissiveTexture.index;

		mat.metal_rough = Vec2{(float)glmat.pbrMetallicRoughness.metallicFactor, (float)glmat.pbrMetallicRoughness.roughnessFactor};
		mat.metal_rough_tex = glmat.pbrMetallicRoughness.metallicRoughnessTexture.index;

		mat.normal_tex = glmat.normalTexture.index;

		std::vector<VK::Mesh::Vertex> verts;
		for(size_t i = 0; i < positions.size(); i++) {
			Vec3 p = positions[i];
			Vec3 n = i < normals.size() ? normals[i] : Vec3{};
			Vec4 t = i < tangents.size() ? tangents[i] : Vec4{};
			Vec2 tc = i < texcoords.size() ? texcoords[i] : Vec2{};
			verts.push_back({Vec4{p, tc.x}, Vec4{n, tc.y}, t});
		}

		add(Object(reserve_id(), pose, VK::Mesh(std::move(verts), std::move(indices)), mat));
	}
}

std::string Scene::load(std::string file, Camera& cam) {

	clear();

	using namespace tinygltf;

	Model model;
	TinyGLTF loader;
	std::string err;
	std::string warn;
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, file);

	if(!warn.empty()) {
		warn("%s\n", warn.c_str());
	}

	if(!err.empty()) {
		warn("Err: %s\n", err.c_str());
	}

	if(!ret) {
		warn("Failed to parse glTF\n");
	}

	std::function<void(int, Mat4)> load_node;
	load_node = [&, this](int n, Mat4 T) {
		
		auto& node = model.nodes[n];
		Mat4 M;
		for(int i = 0; i < 16 && i < node.matrix.size(); i++)
			M.data[i] = (float)node.matrix[i];
		
		T = T * M.T();
		Pose pose;
		T.decompose(pose.pos, pose.scale, pose.euler);
		
		if(node.mesh >= 0)
			parse_mesh(model, model.meshes[node.mesh], pose);

		for(auto& child : node.children) {
			load_node(child, T);
		}
	};

	for(auto& scene : model.scenes) {
		for(auto& root : scene.nodes) {
			load_node(root, Mat4::I);
		}
	}

	// Iterate through all texture declaration in glTF file
	for(const auto &gltfTexture : model.textures) {
		if(gltfTexture.source >= model.images.size()) continue;

		Util::Image tex;
		const auto &image = model.images[gltfTexture.source];
		assert(image.component == 4);

		const auto size =
			image.component * image.width * image.height * sizeof(unsigned char);
		std::vector<unsigned char> data(size);
		std::memcpy(data.data(), image.image.data(), size);

		tex.reload(image.width, image.height, std::move(data));
		textures.push_back(std::move(tex));
	}

	return err;
}

unsigned int Scene::n_textures() const {
	return (unsigned int)textures.size();
}

