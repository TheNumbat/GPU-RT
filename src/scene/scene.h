
#pragma once

#include <string>
#include <unordered_map>

#include "object.h"
#include <lib/mathlib.h>
#include <util/camera.h>
#include <util/image.h>

class Scene {
public:
    Scene(unsigned int start = 1);
    ~Scene() = default;

    std::string load(std::string file, Camera& cam);

    template<typename F> void for_objs(F&& func) {
        for(auto& obj : objs) func(obj.second);
    }
    template<typename F> void for_objs(F&& func) const {
        for(auto& obj : objs) func(obj.second);
    }

    bool empty();
    size_t size() const;
    void clear();
    void erase(unsigned int id);
    unsigned int add(Object&& obj);
    unsigned int reserve_id();
    unsigned int used_ids();
    unsigned int n_textures() const;

    Object& get(unsigned int id);

    const std::vector<Util::Image>& images() const {
        return textures;
    };

private:
    std::unordered_map<unsigned int, Object> objs;
    std::vector<Util::Image> textures;
    unsigned int next_id, first_id;
};
