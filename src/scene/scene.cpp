
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <sstream>

#include "scene.h"

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
}

bool Scene::empty() {
    return objs.size() == 0;
}

Object& Scene::get(unsigned int id) {
    auto entry = objs.find(id);
    assert(entry != objs.end());
    return entry->second;
}

//////////////////////////////////////////////////////////////
// Scene importer/exporter
//////////////////////////////////////////////////////////////

static const std::string FAKE_NAME = "FAKE-S3D-FAKE-MESH";
static const std::string RENDER_CAM_NODE = "S3D-RENDER_CAM_NODE";

static Vec3 aiVec(aiVector3D aiv) {
    return Vec3(aiv.x, aiv.y, aiv.z);
}

static Spectrum aiSpec(aiColor3D aiv) {
    return Spectrum(aiv.r, aiv.g, aiv.b);
}

static Mat4 aiMat(aiMatrix4x4 T) {
    return Mat4{Vec4{T[0][0], T[1][0], T[2][0], T[3][0]}, Vec4{T[0][1], T[1][1], T[2][1], T[3][1]},
                Vec4{T[0][2], T[1][2], T[2][2], T[3][2]}, Vec4{T[0][3], T[1][3], T[2][3], T[3][3]}};
}

static aiMatrix4x4 node_transform(const aiNode* node) {
    aiMatrix4x4 T;
    while(node) {
        T = T * node->mTransformation;
        node = node->mParent;
    }
    return T;
}

static VK::Mesh mesh_from(const aiMesh* mesh) {

    std::vector<VK::Mesh::Vertex> mesh_verts;
    std::vector<VK::Mesh::Index> mesh_inds;

    for(unsigned int j = 0; j < mesh->mNumVertices; j++) {
        const aiVector3D& vpos = mesh->mVertices[j];
        aiVector3D vnorm;
        if(mesh->HasNormals()) {
            vnorm = mesh->mNormals[j];
        }
        mesh_verts.push_back({aiVec(vpos), aiVec(vnorm)});
    }

    for(unsigned int j = 0; j < mesh->mNumFaces; j++) {
        const aiFace& face = mesh->mFaces[j];
        if(face.mNumIndices < 3) continue;
        unsigned int start = face.mIndices[0];
        for(size_t k = 1; k <= face.mNumIndices - 2; k++) {
            mesh_inds.push_back(start);
            mesh_inds.push_back(face.mIndices[k]);
            mesh_inds.push_back(face.mIndices[k + 1]);
        }
    }

    return VK::Mesh(std::move(mesh_verts), std::move(mesh_inds));
}

static Material load_material(aiMaterial* ai_mat) {

    Material mat;

    aiColor3D albedo;
    ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo);
    mat.albedo = aiSpec(albedo);

    aiColor3D emissive;
    ai_mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
    mat.emissive = aiSpec(emissive);

    aiColor3D reflectance;
    ai_mat->Get(AI_MATKEY_COLOR_REFLECTIVE, reflectance);
    mat.reflectance = aiSpec(reflectance);

    aiColor3D transmittance;
    ai_mat->Get(AI_MATKEY_COLOR_TRANSPARENT, transmittance);
    mat.transmittance = aiSpec(transmittance);

    ai_mat->Get(AI_MATKEY_REFRACTI, mat.ior);
    ai_mat->Get(AI_MATKEY_SHININESS, mat.intensity);

    aiString ai_type;
    ai_mat->Get(AI_MATKEY_NAME, ai_type);
    std::string type(ai_type.C_Str());

    if(type.find("lambertian") != std::string::npos) {
        mat.type = Material_Type::lambertian;
    } else if(type.find("mirror") != std::string::npos) {
        mat.type = Material_Type::mirror;
    } else if(type.find("refract") != std::string::npos) {
        mat.type = Material_Type::refract;
    } else if(type.find("glass") != std::string::npos) {
        mat.type = Material_Type::glass;
    } else if(type.find("diffuse_light") != std::string::npos) {
        mat.type = Material_Type::diffuse_light;
    } else {
        mat = Material();
    }
    mat.emissive *= 1.0f / mat.intensity;

    return mat;
}

static void load_node(Scene& scobj, std::vector<std::string>& errors,
                      std::unordered_map<aiNode*, unsigned int>& node_to_obj, const aiScene* scene,
                      aiNode* node, aiMatrix4x4 transform) {

    transform = transform * node->mTransformation;

    for(unsigned int i = 0; i < node->mNumMeshes; i++) {

        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        aiVector3D ascale, arot, apos;
        transform.Decompose(ascale, arot, apos);
        Vec3 pos = aiVec(apos);
        Vec3 rot = aiVec(arot);
        Vec3 scale = aiVec(ascale);
        Pose p = {pos, Degrees(rot).range(0.0f, 360.0f), scale};

        Material mat_opt = load_material(scene->mMaterials[mesh->mMaterialIndex]);

        VK::Mesh gmesh = mesh_from(mesh);

        Object new_obj(scobj.reserve_id(), p, std::move(gmesh));

        new_obj.material = mat_opt;

        node_to_obj[node] = new_obj.id();
        scobj.add(std::move(new_obj));
    }

    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        load_node(scobj, errors, node_to_obj, scene, node->mChildren[i], transform);
    }
}

static unsigned int load_flags(Scene::Load_Opts opt) {

    unsigned int flags = aiProcess_OptimizeMeshes | aiProcess_FindInvalidData |
                         aiProcess_FindInstances | aiProcess_FindDegenerates;

    if(opt.drop_normals) {
        flags |= aiProcess_DropNormals;
    }
    if(opt.join_verts) {
        flags |= aiProcess_JoinIdenticalVertices;
    }
    if(opt.triangulate) {
        flags |= aiProcess_Triangulate;
    }
    if(opt.gen_smooth_normals) {
        flags |= aiProcess_GenSmoothNormals;
    } else if(opt.gen_normals) {
        flags |= aiProcess_GenNormals;
    }
    if(opt.fix_infacing_normals) {
        flags |= aiProcess_FixInfacingNormals;
    }
    if(opt.debone) {
        flags |= aiProcess_Debone;
    } else {
        flags |= aiProcess_PopulateArmatureData;
    }

    return flags;
}

static const std::string ANIM_CAM_NAME = "S3D-ANIM_CAM";

std::string Scene::load(Scene::Load_Opts loader, std::string file, Camera& cam) {

    if(loader.new_scene) {
        clear();
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(file.c_str(), load_flags(loader));

    if(!scene) {
        return "Parsing scene " + file + ": " + std::string(importer.GetErrorString());
    }

    std::vector<std::string> errors;
    std::unordered_map<aiNode*, unsigned int> node_to_obj;
    scene->mRootNode->mTransformation = aiMatrix4x4();

    // Load objects
    load_node(*this, errors, node_to_obj, scene, scene->mRootNode, aiMatrix4x4());

    // Load cameras
    if(loader.new_scene && scene->mNumCameras > 0) {

        auto load = [&](unsigned int i) {
            const aiCamera& aiCam = *scene->mCameras[i];
            Mat4 cam_transform = aiMat(node_transform(scene->mRootNode->FindNode(aiCam.mName)));
            Vec3 pos = cam_transform * aiVec(aiCam.mPosition);
            Vec3 center = cam_transform * aiVec(aiCam.mLookAt);

            std::string name(aiCam.mName.C_Str());
            if(name.find(ANIM_CAM_NAME) == std::string::npos) {
                cam.load(pos, center, aiCam.mAspect, aiCam.mHorizontalFOV, aiCam.mClipPlaneNear,
                         aiCam.mClipPlaneFar);
            }
        };

        for(unsigned int i = 0; i < scene->mNumCameras; i++) {
            load(i);
        }
    }

    std::stringstream stream;
    for(size_t i = 0; i < errors.size(); i++) {
        stream << "Loading mesh " << i << ": " << errors[i] << std::endl;
    }
    return stream.str();
}
