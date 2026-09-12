#pragma once

// Populates the asset libraries with procedural/code-generated assets
// (debug textures, default checkerboard, code materials, quads, etc.).
// Must be called after AssetManager::RegisterLib<...>() and before LoadScene()
// so that scene JSON can reference these assets by name.
void CreateCodeAssets();
