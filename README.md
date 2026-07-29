#Xtensa: Vulkan-Renderer

This is close to a deployable state, but I haven't really tested how well it works by just downloading and building...

Named this "Xtensa" after Descartes' Res Extensa--"a thing extended in space". Computer code, such as scene logic, represents a formal abstract world and renderers transform this logical world into perceivable space

Work-in-progress. I've done a lot of work on architecture, but I've moved on to visual quality. The Sys-Core Gfx-Core repos are essential for this repo.

Features:
* High-level abstractions (new render techniques can be made fully implemented in 100-200 LOC)
* Depth-prepass
* Shadow maps
* PBR (Epic's precomputed BRDF, IBL, GGX)
* PBR Bloom
* Auto-exposure
* Bokeh Depth-of-Field
* SSAO
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
* Visibility Buffer + Deferred Lighting
