# Extensa: Vulkan-Renderer

This is close to a deployable state, but I haven't really tested how well it works by just downloading and building...

Calling this "Extensa" after Descartes Res Extensa--"a thing extended in space". I've always conceived of 3D rendering as giving shape to abstract logical worlds so felt the name fit.

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
* Stencil effects
* Asset and scene management
* Model instancing
* Runtime Object manipulation
* Shader hotswap
* MSAA
* Env-maps

To Do:
* Raytraced reflections
* Bokeh DoF
* SSAO
