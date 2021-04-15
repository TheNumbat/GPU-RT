
#pragma once

#include <string>
#include <unordered_map>

#include "object.h"
#include <lib/mathlib.h>
#include <util/camera.h>

class Scene {
public:
    Scene(unsigned int start = 1);
    ~Scene() = default;

    struct Load_Opts {
        bool new_scene = false;
        bool drop_normals = false;
        bool join_verts = true;
        bool triangulate = false;
        bool gen_normals = true;
        bool gen_smooth_normals = false;
        bool fix_infacing_normals = false;
        bool debone = false;
    };

    std::string load(Load_Opts opt, std::string file, Camera& cam);

    template<typename F> void for_objs(F&& func) {
        for(auto& obj : objs) func(obj.second);
    }

    bool empty();
    size_t size();
    void clear();
    void erase(unsigned int id);
    unsigned int add(Object&& obj);
    unsigned int reserve_id();
    unsigned int used_ids();

    Object& get(unsigned int id);

private:
    std::unordered_map<unsigned int, Object> objs;
    unsigned int next_id, first_id;
};
