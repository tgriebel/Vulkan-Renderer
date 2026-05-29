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
			return pipelineType_t::RT_GEN;

		case RT_CLOSEST_HIT:
		case RT_ANY_HIT:
		case RT_MISS:
		case RT_INTERSECTION:
			return pipelineType_t::RT_HIT_GROUP;

		case RT_CALLABLE:
			return pipelineType_t::RT_CALLABLE;
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

	if( ext2 == "hlsl" )
	{
		SplitFileName( baseName, name, ext );
	}
	else
	{
		name = baseName;
		ext = ext2;
	}

	if ( ext == "vert" || ext == "vs" ) {
		name += "VS";
	}
	else if ( ext == "frag" || ext == "ps" ) {
		name += "PS";
	}
	else if ( ext == "comp" || ext == "cs" ) {
		name += "CS";
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


bool GpuProgramLoader::LoadRasterProgram( GpuProgram& program )
{
	assert( permIdCount >= 1 );

	program.type = pipelineType_t::RASTER;
	program.shaderCount = 2;
	program.bindsetCount = 0;
	program.permSet = shaderPermId_t::NONE;

	shaderPermId_t permPowerSet[ GpuProgram::MaxPermutations ]; // Array of all set combinations
	uint32_t permSetCount = 1; // Always 1+ b/c of "NONE"

	for ( uint32_t permIndex = 0; permIndex < GpuProgram::MaxPermutations; ++permIndex )
	{
		permPowerSet[ permIndex ] = shaderPermId_t::NONE;
	}

	// Build superset of all permutations
	shaderPermId_t superSet = shaderPermId_t::NONE;
	for( uint32_t permIndex = 0; permIndex < permIdCount; ++permIndex )
	{
		superSet |= permList[ permIndex ];
	}

	// Build list of valid sets (i.e. powerset)
	for ( uint32_t permIndex = 1; permIndex < GpuProgram::MaxPermutations; ++permIndex )
	{
		shaderPermId_t permSet = shaderPermId_t( permIndex );
		if ( ( superSet & permSet ) != permSet ) {
			continue;
		}

		permPowerSet[ permSetCount ] = permSet;
		++permSetCount;
	}

	program.permCount = permSetCount;

	// Go through the powerset and compile each specific combination
	for ( uint32_t permSetIndex = 0; permSetIndex < permSetCount; ++permSetIndex )
	{
		const std::string vsBinName = GetBinName( vsFileName, permPowerSet[ permSetIndex ] );
		const std::string psBinName = GetBinName( psFileName, permPowerSet[ permSetIndex ] );

		CheckCompileShader( srcPath + vsFileName, binPath + vsBinName, permPowerSet[ permSetIndex ], HasFlags( LOAD_HANDLER_FLAGS_REBAKE ) );
		CheckCompileShader( srcPath + psFileName, binPath + psBinName, permPowerSet[ permSetIndex ], HasFlags( LOAD_HANDLER_FLAGS_REBAKE ) );

		const uint32_t shaderPermIndex = static_cast<uint32_t>( permPowerSet[ permSetIndex ] );
		ShaderSource& vs = program.shaders[ 0 ];
		ShaderBin& vsBin = program.shaderBins[ 0 ][ shaderPermIndex ];

		vs.name = vsFileName;
		vs.src = ReadTextFile( srcPath + vsFileName );
		vs.type = shaderType_t::VERTEX;

		vsBin.binName = vsBinName;
		vsBin.blob = ReadBinaryFile( binPath + vsBinName );
		vsBin.type = shaderType_t::VERTEX;

		ShaderSource& ps = program.shaders[ 1 ];
		ShaderBin& psBin = program.shaderBins[ 1 ][ shaderPermIndex ];

		ps.name = psFileName;
		ps.src = ReadTextFile( srcPath + psFileName );
		ps.type = shaderType_t::PIXEL;

		psBin.binName = psBinName;
		psBin.blob = ReadBinaryFile( binPath + psBinName );
		psBin.type = shaderType_t::PIXEL;
	}

	return true;
}


bool GpuProgramLoader::LoadSingleProgram( GpuProgram& program )
{
	const shaderType_t shaderType = GetTypeFromName( srcFileName );
	const pipelineType_t pipelineType = GetPipelineTypeForShaderType( shaderType );

	program.type = pipelineType;
	program.shaderCount = 1;
	program.bindsetCount = 0;
	program.permCount = permIdCount;
	program.permSet = shaderPermId_t::NONE;

	const std::string binName = GetBinName( srcFileName, shaderPermId_t::NONE );

	CheckCompileShader( srcPath + srcFileName, binPath + binName, shaderPermId_t::NONE, HasFlags( LOAD_HANDLER_FLAGS_REBAKE ) );

	ShaderSource& source = program.shaders[ 0 ];
	ShaderBin& bin = program.shaderBins[ 0 ][ 0 ];

	source.name = srcFileName;
	source.src = ReadTextFile( srcPath + srcFileName );
	source.type = shaderType;

	bin.binName = binName;
	bin.blob = ReadBinaryFile( binPath + binName );
	bin.type = shaderType;
#ifdef USE_VULKAN
	bin.vk_shader = VK_NULL_HANDLE;
#endif

	return true;
}


bool GpuProgramLoader::Load( Asset<GpuProgram>& programAsset )
{
	GpuProgram& program = programAsset.Get();

	program.bindHash = bindHash;
	program.flags = flags;

	if ( ( !vsFileName.empty() ) && ( !psFileName.empty() ) )
	{
		return LoadRasterProgram( program );
	}
	else if ( !srcFileName.empty() )
	{
		return LoadSingleProgram( program );
	}
	return false;
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


void GpuProgramLoader::AddFilePaths( const std::string& vertexFileName, const std::string& pixelFileName, const std::string& computeFileName )
{
	if ( !vertexFileName.empty() ) {
		vsFileName = vertexFileName + ".vs.hlsl";
	}
	if ( !pixelFileName.empty() ) {
		psFileName = pixelFileName + ".ps.hlsl";
	}
	if ( !computeFileName.empty() ) {
		srcFileName = computeFileName + ".cs.hlsl";
	}
}
