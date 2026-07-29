#Xtensa: Vulkan-Renderer

This is close to a deployable state, but I haven't really tested how well it works by just downloading and building...

Named this "Xtensa" after Descartes Res Extensa--"a thing extended in space". Computer code, such as scene logic, represents a formal abstract world and renderers transform this into relatable space

Work-in-progress. I've done a lot of work on architecture, but I've moved on to visual quality. The Sys-Core Gfx-Core repos are essential for this repo.

Features:
* Depth-prepass
* Shadow maps
* PBR (Epic's precomputed BRDF, IBL, GGX)
* Bloom
* Luminance
* Depth-of-Field
* Transparents
* Post-process
* Multiple shaders (+ hot reloading)
* Shader permutations
* Stencil effects
* Asset and scene management
* Model instancing
* Runtime Object manipulation
* MSAA
* Env-maps
* MRT

To Do:
* Raytraced reflections
* Bokeh DoF
* SSAO
* Visibility Buffer + Deferred Lighting
