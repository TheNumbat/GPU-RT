
# GPU-RT

This is a real-time path tracer based on Vulkan 1.2's cross platform ray tracing extension. It implements a few different material models and integrators, and can load GLTF scenes. In particular, it implements ReSTIR for direct lighting.

## FCPW-GPU

The FCPW-GPU branch contains my work on implementing fast GPU closest point queries for [FCPW](https://github.com/rohan-sawhney/fcpw). It uses Vulkan compute shaders to test a variety of BVH construction and traversal strategies. 

## Build

The build process is the same as [Scotty3D](https://cmu-graphics.github.io/Scotty3D/build/) and should work as expected. Once the executable is built, it can be run with the arguments `-s media/cbox/cbox.gltf` to open the cornell box scene (this can also be done in the GUI).

Dependencies for windows are all included, except `glslc` (the SPIR-V shader compiler). On macOS/linux, it will also require `libsdl2` and `libvulkan-dev`. 

![showcase render](https://raw.githubusercontent.com/TheNumbat/GPU-RT/main/render.png)
