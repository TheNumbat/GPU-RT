
#include "object.h"

Object::Object(unsigned int id, Pose p, VK::Mesh&& m, Material mat) : pose(p), _id(id), _mesh(std::move(m)), material(mat) {
}

unsigned int Object::id() const {
    return _id;
}

const VK::Mesh& Object::mesh() const {
    return _mesh;
}

void Object::render(const Mat4& view) {
}
