#include "gpuProgram.h"
#include "../render_state/rhi.h"

using namespace SysCore;

struct shaderFileTypeMapPair_t
{
	const char*		ext;
	shaderType_t	type;
};


static shaderFileTypeMapPair_t s_fileExtTypeMap[ shaderType_t::COUNT ] =
{
	{ "vs",		shaderType_t::VERTEX },
	{ "ps",		shaderType_t::PIXEL },
	{ "cs",		shaderType_t::COMPUTE },
	{ "rgen",	shaderType_t::RT_GEN },
	{ "rint",	shaderType_t::RT_INTERSECTION },
	{ "rchit",	shaderType_t::RT_CLOSEST_HIT },
	{ "rahit",	shaderType_t::RT_ANY_HIT },
	{ "rmiss",	shaderType_t::RT_MISS },
	{ "rcall",	shaderType_t::RT_CALLABLE }
};


static shaderType_t GetTypeFromName( const std::string& fileName )
{
	std::string name;
	std::string baseName;
	std::string ext;
	std::string ext2;
	SplitFileName( fileName, baseName, ext2 );

	assert( ext2 == "hlsl" );
	SplitFileName( baseName, name, ext );

	for( uint32_t i = 0; i < shaderType_t::COUNT; ++i )
	{
		if( ext == s_fileExtTypeMap[ i ].ext ) {
			return s_fileExtTypeMap[ i ].type;
		}
	}
	return shaderType_t::UNSPECIFIED;
}


static const char* GetExtFromType( const shaderType_t type )
{
	for( uint32_t i = 0; i < shaderType_t::COUNT; ++i )
	{
		if( type == s_fileExtTypeMap[ i ].type ) {
			return s_fileExtTypeMap[ i ].ext;
		}
	}
	return nullptr;
}


static pipelineType_t GetPipelineTypeForShaderType( const shaderType_t type )
{
	switch( type )
	{
		case VERTEX:
		case PIXEL:
			return pipelineType_t::RASTER;

		case COMPUTE:
			return pipelineType_t::COMPUTE;

		case RT_GEN:
		case RT_CLOSEST_HIT:
		case RT_ANY_HIT:
		case RT_INTERSECTION:
		case RT_MISS:
		case RT_CALLABLE:
			return pipelineType_t::RAY_TRACING;
	}
	assert( 0 );
	return pipelineType_t::UNSPECIFIED;
}


void GpuProgram::CreateApiObjects()
{
	for( uint32_t shaderIx = 0; shaderIx < shaderCount; ++shaderIx )
	{
		GpuProgram::ShaderPermMap& shaderMap = shaderBins[ shaderIx ];
		for( auto permIt = shaderMap.begin(); permIt != shaderMap.end(); ++permIt )
		{
#ifdef USE_VULKAN
			permIt->second.vk_shader = vk_CreateShaderModule( permIt->second.blob, permIt->second.binName.c_str() );
#endif
		}
	}
}


void GpuProgram::DestroyApiObjects()
{
	for( uint32_t shaderIx = 0; shaderIx < shaderCount; ++shaderIx )
	{
		for( auto perm : shaderBins[ shaderIx ] )
		{
			ShaderBin& shaderBin = perm.second;
#ifdef USE_VULKAN
			if( shaderBin.vk_shader != VK_NULL_HANDLE )
			{
				vkDestroyShaderModule( context.device, shaderBin.vk_shader, nullptr );
			}
#endif
		}
	}
}


std::string GpuProgramLoader::GetBinName( const std::string& fileName, const shaderPermId_t permSet )
{
	std::string name;
	std::string baseName;
	std::string ext;
	std::string ext2;
	SplitFileName( fileName, baseName, ext2 );
	assert( ext2 == "hlsl" );
	SplitFileName( baseName, name, ext );

	for ( uint32_t i = 0; i < shaderType_t::COUNT; ++i )
	{
		if ( s_fileExtTypeMap[ i ].ext != nullptr && ext == s_fileExtTypeMap[ i ].ext )
		{
			ToUpper( ext );
			name += ext;
			break;
		}
	}

	for ( uint32_t i = 0; i < shaderPermId_t::COUNT; ++i )
	{
		const uint32_t permBit = static_cast<uint32_t>( permSet ) & ( 1u << i );
		if ( permBit == 0 ) {
			continue;
		}
		const shaderPermId_t permId = static_cast<shaderPermId_t>( permBit );

		const shaderPerm_t* perm = FindPerm( permId );
		if ( perm != nullptr ) {
			name += "_" + perm->tag;
		}
	}

	name += ".spv";

	return name;
}


std::string GpuProgramLoader::GetCompileString( const std::string& srcPath, const std::string& binPath, const std::string& perms )
{
	// Extract just the filename from srcPath (e.g. "resolve.ps.hlsl")
	std::string filename = srcPath;
	const size_t lastSlash = srcPath.find_last_of( "\\/" );
	if ( lastSlash != std::string::npos ) {
		filename = srcPath.substr( lastSlash + 1 );
	}

	std::string cmd = "python " + compilerPath + "shader_compiler.py";

	// Perms are comma-delimited (e.g. -p msaa,skycube)
	if ( !perms.empty() ) {
		cmd += " -p " + perms;
	}

	cmd += " " + filename;

	return cmd;
}


void GpuProgramLoader::CheckCompileShader( const std::string& path, const std::string& binPath, const shaderPermId_t permSet, const bool forceRebuild )
{
	if ( FileExists( binPath ) == false || forceRebuild )
	{
		std::string perms = "";

		for ( uint32_t i = 0; i < shaderPermId_t::COUNT; ++i )
		{
			const uint32_t permBit = static_cast<uint32_t>( permSet ) & ( 1u << i );
			if( permBit == 0 ) {
				continue;
			}
			const shaderPermId_t permId = static_cast<shaderPermId_t>( permBit );

			const shaderPerm_t* shaderPerm = FindPerm( permId );
			if ( shaderPerm != nullptr ) {
				if ( !perms.empty() ) {
					perms += ",";
				}
				perms += shaderPerm->tag;
			}
		}

		std::string compileCommand = GetCompileString( path, binPath, perms );
		system( compileCommand.c_str() );
	}
}


bool GpuProgramLoader::Load( Asset<GpuProgram>& programAsset )
{
	GpuProgram& program = programAsset.Get();
	program.bindHash = bindHash;
	program.flags = flags;
	program.shaderCount = 0;
	program.bindsetCount = 0;

	// Collect active shader slots in map order
	struct ShaderSlot
	{
		std::string fileName;
		shaderType_t type;
	};
	ShaderSlot activeShaders[ shaderType_t::COUNT ] = {};
	uint32_t activeCount = 0;

	for ( uint32_t i = 0; i < shaderType_t::COUNT; ++i )
	{
		if ( s_fileExtTypeMap[ i ].ext == nullptr ) {
			continue;
		}
		const shaderType_t type = s_fileExtTypeMap[ i ].type;

		const std::string& baseName = fileNames.names[ type ];
		if ( baseName.empty() ) {
			continue;
		}
		activeShaders[ activeCount++ ] = { baseName + "." + s_fileExtTypeMap[ i ].ext + ".hlsl", type };
	}

	if ( activeCount == 0 ) {
		return false;
	}

	program.type = GetPipelineTypeForShaderType( activeShaders[ 0 ].type );
	program.shaderCount = activeCount;

	// Build permutation superset and powerset
	shaderPermId_t superSet = shaderPermId_t::NONE;
	for ( uint32_t i = 0; i < permIdCount; ++i ) {
		superSet |= permList[ i ];
	}

	shaderPermId_t permPowerSet[ GpuProgram::MaxPermutations ];
	uint32_t permSetCount = 1;
	permPowerSet[ 0 ] = shaderPermId_t::NONE;

	for ( uint32_t permIndex = 1; permIndex < GpuProgram::MaxPermutations; ++permIndex )
	{
		const shaderPermId_t perm = shaderPermId_t( permIndex );
		if ( ( superSet & perm ) != perm ) {
			continue;
		}
		permPowerSet[ permSetCount++ ] = perm;
	}

	program.permCount = permSetCount;
	program.permSet = superSet;

	// Load shader sources (same across all permutations)
	for ( uint32_t shaderIx = 0; shaderIx < activeCount; ++shaderIx )
	{
		ShaderSource& source = program.shaders[ shaderIx ];
		source.name = activeShaders[ shaderIx ].fileName;
		source.src = ReadTextFile( srcPath + activeShaders[ shaderIx ].fileName );
		source.type = activeShaders[ shaderIx ].type;
	}

	// Compile and load binaries for each permutation of each shader
	for ( uint32_t permSetIndex = 0; permSetIndex < permSetCount; ++permSetIndex )
	{
		const shaderPermId_t perm = permPowerSet[ permSetIndex ];
		const uint32_t permKey = static_cast<uint32_t>( perm );

		for ( uint32_t shaderIx = 0; shaderIx < activeCount; ++shaderIx )
		{
			const std::string& fileName = activeShaders[ shaderIx ].fileName;
			const shaderType_t shaderType = activeShaders[ shaderIx ].type;
			const std::string binName = GetBinName( fileName, perm );

			CheckCompileShader( srcPath + fileName, binPath + binName, perm, HasFlags( LOAD_HANDLER_FLAGS_REBAKE ) );

			ShaderBin& bin = program.shaderBins[ shaderIx ][ permKey ];
			bin.binName = binName;
			bin.blob = ReadBinaryFile( binPath + binName );
			bin.type = shaderType;
#ifdef USE_VULKAN
			bin.vk_shader = VK_NULL_HANDLE;
#endif
		}
	}

	return true;
}


void GpuProgramLoader::SetSourcePath( const std::string& path )
{
	srcPath = path;
}


void GpuProgramLoader::SetBinPath( const std::string& path )
{
	binPath = path;
}


void GpuProgramLoader::SetBindSet( const std::string& setName )
{
	bindHash = SysCore::Hash( setName );
}


void GpuProgramLoader::AddPerm( const std::string& permName )
{
	if ( permIdCount > static_cast<uint32_t>( shaderPermId_t::COUNT ) ) {
		return;
	}

	shaderPermId_t permId = GetPermId( permName );

	if ( permId == shaderPermId_t::NONE ) {
		return;
	}

	permList[ permIdCount ] = permId;
	++permIdCount;
}


void GpuProgramLoader::SetFlags( const shaderFlags_t shaderFlags )
{
	flags = shaderFlags;
}


void GpuProgramLoader::SetCompilerPath( const std::string& path )
{
	compilerPath = path;
}


void GpuProgramLoader::AddFilePaths( const shaderFileNames_t& fileNames )
{
	this->fileNames = fileNames;
}
