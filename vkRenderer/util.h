#pragma once
#include "common.h"
#include "GeoBuilder.h"

glm::mat4 MatrixFromVector( const glm::vec3& v );
bool LoadTextureImage( const char* texturePath, texture_t& texture );
void CreateShaders( GpuProgram& prog );
void CopyGeoBuilderResult( const GeoBuilder& gb, std::vector<VertexInput>& vb, std::vector<uint32_t>& ib );
void CreateSkyBoxSurf( modelSource_t& outModel );
void CreateTerrainSurface( modelSource_t& outModel );
void CreateWaterSurface( modelSource_t& outModel );
void CreateQuadSurface2D( const std::string& materialName, modelSource_t& outModel, glm::vec2& origin, glm::vec2& size );
void CreateStaticModel( const std::string& modelName, const std::string& materialName, modelSource_t& outModel );