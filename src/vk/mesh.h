
#pragma once

#include <lib/mathlib.h>
#include <util/camera.h>
#include <vector>

#include "vulkan.h"

class Scene;

namespace VK {

struct Mesh {
    typedef unsigned int Index;
    struct Vertex {
        Vec4 pos;
        Vec4 norm;
        Vec4 tang;
        static VkVertexInputBindingDescription bind_desc();
        static std::array<VkVertexInputAttributeDescription, 3> attr_descs();
    };

    Mesh() = default;
    Mesh(std::vector<Vertex>&& vertices, std::vector<Index>&& indices);
    Mesh(const Mesh& src) = delete;
    Mesh(Mesh&& src);
    ~Mesh() = default;

    Mesh& operator=(const Mesh& src) = delete;
    Mesh& operator=(Mesh&& src);

    void recreate(std::vector<Vertex>&& vertices, std::vector<Index>&& indices);
    void render(VkCommandBuffer& cmds, const PipeData& pipe, const Mat4& T = Mat4::I) const;
    void sync() const;

    const std::vector<Vertex>& verts() const {
        return _verts;
    }
    const std::vector<Index>& inds() const {
        return _idxs;
    }

private:
    std::vector<Vertex> _verts;
    std::vector<Index> _idxs;
    mutable Drop<Buffer> vbuf, ibuf;
    mutable bool dirty = false;

    friend struct Accel;
    friend struct MeshPipe;
    friend struct RTPipe;
};

struct MeshPipe {

    MeshPipe() = default;
    MeshPipe(const Pass& pass, VkExtent2D ext);
    ~MeshPipe();

    MeshPipe(const MeshPipe&) = delete;
    MeshPipe(MeshPipe&& src) = default;
    MeshPipe& operator=(const MeshPipe&) = delete;
    MeshPipe& operator=(MeshPipe&& src) = default;

    void recreate(const Pass& pass, VkExtent2D ext);
    void destroy();

    void recreate_swap(const Pass& pass, VkExtent2D ext);

    void update_uniforms(const Camera& cam);

    Drop<PipeData> pipe;

private:
    friend struct Mesh;

    std::vector<Drop<Buffer>> camera_uniforms;

    void create_pipe(const Pass& pass, VkExtent2D ext);
    void create_desc();
};

} // namespace VK
