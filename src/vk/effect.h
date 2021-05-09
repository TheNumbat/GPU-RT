
#pragma once

#include <lib/mathlib.h>
#include "vulkan.h"

namespace VK {

struct EffectPipe {
    
    EffectPipe() = default;
    EffectPipe(const Pass& pass, VkExtent2D ext);
    ~EffectPipe();

    EffectPipe(const EffectPipe&) = delete;
    EffectPipe(EffectPipe&& src) = default;
    EffectPipe& operator=(const EffectPipe&) = delete;
    EffectPipe& operator=(EffectPipe&& src) = default;

    void recreate(const Pass& pass, VkExtent2D ext);
    void destroy();
    void recreate_swap(const Pass& pass, VkExtent2D ext);
    void tonemap(VkCommandBuffer& cmds, const ImageView& image);

    float exposure = 1.0f;
    float gamma = 2.2f;
    int tonemap_type = 0;

private:
    struct Push_Consts {
        float exposure;
        float gamma;
        int type;
    };

    Push_Consts consts;
    Drop<PipeData> pipe;
    Drop<Sampler> sampler;

    void create_pipe(const Pass& pass, VkExtent2D ext);
    void create_desc();
};

} // namespace VK
