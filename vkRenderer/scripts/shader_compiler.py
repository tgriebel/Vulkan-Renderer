import os
import sys
import json
import subprocess
from pathlib import Path
from dataclasses import dataclass, field
from datetime import datetime

# Parses a json file with shader entries. The entries represent a shaders bound to a given pipeline (both VS and PS, etc)
# Shaders are tightly coupled this way due to input/output from VS and PS shaders which is why they are collected
# The compiler, however, works on individual files so this parses the json, redupes individual files, and builds a compile command

# Usage:

# python shader_compiler.py scenes/              # full build from directory
# python shader_compiler.py scene.json           # full build from single json
# python shader_compiler.py scene.json resolve   # single shader compile (by stem name)
# python shader_compiler.py -p <permutation>,<permutation> simple.ps.hlsl       # single shader compile (by file)


# Config
BIN_DIR    = Path( __file__ ).resolve().parent.parent.parent / "external" / "vulkan" / "Bin"
DXC        = str( BIN_DIR / "dxc.exe" )
GLSLANG    = str( BIN_DIR / "glslangValidator.exe" )
SHADER_DIR = "..\\shaders\\"
OUT_DIR    = "..\\shaders_bin\\"
LOG_FILE   = "shader_build.log"

FLAG_MAP = {
    "msaa"    : "USE_MSAA",
    "skycube" : "USE_CUBE_SAMPLER",
}

# ---- HLSL (DXC) configuration ----

HLSL_TYPE_EXT = {
    "vs" : "vs.hlsl",
    "ps" : "ps.hlsl",
    "cs" : "cs.hlsl",
}

HLSL_TYPE_SUFFIX = {
    "vs.hlsl" : "VS",
    "ps.hlsl" : "PS",
    "cs.hlsl" : "CS",
}

HLSL_PROFILE = {
    "vs.hlsl" : "vs_6_0",
    "ps.hlsl" : "ps_6_0",
    "cs.hlsl" : "cs_6_0",
}

HLSL_ENTRY = {
    "vs.hlsl" : "VSMain",
    "ps.hlsl" : "PSMain",
    "cs.hlsl" : "CSMain",
}

# ---- GLSL (glslangValidator) configuration (legacy) ----

GLSL_TYPE_EXT = {
    "vs" : "vert",
    "ps" : "frag",
    "cs" : "comp",
}

GLSL_TYPE_SUFFIX = {
    "vert" : "VS",
    "frag" : "PS",
    "comp" : "CS",
}


@dataclass( frozen=True )
class ShaderRecord:
    source : str
    output : str
    ext    : str
    macros : tuple = field( default_factory=tuple )


# Combines all json shaders from all scenes.
# This way scenes can include specialized shaders and still have them compiled
def load_all_shaders( directory: Path ) -> dict:
    all_shaders = []
    for json_file in directory.glob( "*.json" ):
        print( f"Loading {json_file.name}" )
        with open( json_file, "r" ) as f:
            try:
                data = json.load( f )
                if "shaders" in data:
                    all_shaders.extend( data[ "shaders" ] )
            except json.JSONDecodeError as e:
                print( f"WARNING: Failed to parse {json_file.name}: {e}" )
    return { "shaders": all_shaders }


def strip_shader_ext( name: str ) -> str:
    for ext in ( ".vert", ".frag", ".comp", ".vs.hlsl", ".ps.hlsl", ".cs.hlsl" ):
        if name.endswith( ext ):
            return name[ :-len( ext ) ]
    return name


def is_hlsl_mode():
    return "--glsl" not in sys.argv


# Produce single file records for each shader type
def parse_shaders( data ) -> list[ ShaderRecord ]:
    hlsl     = is_hlsl_mode()
    type_ext = HLSL_TYPE_EXT if hlsl else GLSL_TYPE_EXT
    type_sfx = HLSL_TYPE_SUFFIX if hlsl else GLSL_TYPE_SUFFIX

    records = []
    for shader in data[ "shaders" ]:
        attribs = shader.keys()

        sources = []
        for key in ( "vs", "ps", "cs" ):
            if key in attribs:
                sources.append( ( strip_shader_ext( shader[ key ] ), type_ext[ key ] ) )

        macros      = []
        perm_suffix = ""
        perms       = shader.get( "perm", [] )
        if isinstance( perms, str ):
            perms = [ perms ]

        for perm in perms:
            if perm not in FLAG_MAP:
                print( f"WARNING: Unknown perm '{perm}'" )
                continue
            macros.append( FLAG_MAP[ perm ] )
            perm_suffix += f"_{perm}"

        for stem, ext in sources:
            type_suffix = type_sfx.get( ext, "" )
            source      = f"{stem}.{ext}"
            output      = f"{OUT_DIR}{stem}{type_suffix}{perm_suffix}.spv"

            records.append( ShaderRecord(
                source = SHADER_DIR + source,
                output = output,
                ext    = ext,
                macros = tuple( macros ),
            ) )

    return records


def deduplicate( records: list[ ShaderRecord ] ) -> list[ ShaderRecord ]:
    seen   = set()
    result = []
    for record in records:
        if record not in seen:
            seen.add( record )
            result.append( record )
    return result


def build_command( record: ShaderRecord ) -> list[ str ]:
    if record.ext in HLSL_PROFILE:
        # DXC HLSL -> SPIR-V
        profile = HLSL_PROFILE[ record.ext ]
        entry   = HLSL_ENTRY[ record.ext ]
        cmd = [
            DXC,
            "-spirv",
            "-T", profile,
            "-E", entry,
            "-fvk-use-gl-layout",
            "-fspv-target-env=vulkan1.2",
            record.source,
            "-Fo", record.output,
            "-Zi",                          # debug info
            "-HV", "2021",                  # HLSL version
            "-I", SHADER_DIR,               # include path for headers
        ]
        for macro in record.macros:
            cmd += [ "-D", macro ]
    else:
        # glslangValidator GLSL -> SPIR-V (legacy path)
        cmd = [ GLSLANG, "-l", "-V", record.source, "-o", record.output, "-g" ]
        for macro in record.macros:
            cmd += [ "--define-macro", macro ]
    return cmd


def compile_single_shader_source( source: str, perms: tuple, macros: tuple, log ) -> bool:
    path = Path( source )

    # Determine extension: could be ".ps.hlsl" (two-part) or ".frag" (one-part)
    if path.name.endswith( ".vs.hlsl" ) or path.name.endswith( ".ps.hlsl" ) or path.name.endswith( ".cs.hlsl" ):
        ext  = path.name.split( ".", 1 )[ 1 ]  # e.g. "ps.hlsl"
        stem = path.name.split( ".", 1 )[ 0 ]   # e.g. "resolve"
        type_suffix = HLSL_TYPE_SUFFIX.get( ext, "" )
    else:
        ext  = path.suffix.lstrip( "." )
        stem = path.stem
        type_suffix = GLSL_TYPE_SUFFIX.get( ext, "" )

    # Build perm suffix from perm names for output filename
    perm_suffix = ""
    for perm in perms:
        perm_suffix += f"_{perm}"

    output = f"{OUT_DIR}{stem}{type_suffix}{perm_suffix}.spv"

    record = ShaderRecord(
        source = SHADER_DIR + source,
        output = output,
        ext    = ext,
        macros = macros,
    )
    return compile_record( record, log )


def compile_record( record: ShaderRecord, log ) -> bool:
    cmd    = build_command( record )
    if(record.macros):
        label = f"{record.source} -> {record.output} [{' '.join( record.macros )}]"
    else:
        label = f"{record.source} -> {record.output}"

    log.write( f"Compiling {label}\n" )
    log.write( " ".join( cmd ) + "\n" )

    result = subprocess.run( cmd, capture_output=True, text=True )
    output = result.stdout + result.stderr

    log.write( output + "\n" )

    if result.returncode != 0:
        print( f"ERROR: {label}" )
        print( output )
        return False
    else:
        print( f"OK: {label}" )
        return True


def main():
    os.chdir( Path( __file__ ).parent )

    # Parse -p/--perm flags and other args from argv
    # Perms are comma-delimited: -p msaa,skycube
    args       = []
    cli_perms  = []
    argv_iter  = iter( sys.argv[1:] )
    for arg in argv_iter:
        if arg in ( "-p", "--perm" ):
            perm_str = next( argv_iter, None )
            if perm_str:
                cli_perms.extend( p.strip() for p in perm_str.split( "," ) if p.strip() )
        elif arg.startswith( "--" ) and arg != "--glsl":
            pass                                     # skip unknown flags
        else:
            args.append( arg )

    # Resolve perm names to macros
    cli_macros = []
    for perm in cli_perms:
        if perm not in FLAG_MAP:
            print( f"WARNING: Unknown perm '{perm}'" )
            continue
        cli_macros.append( FLAG_MAP[ perm ] )

    if len( args ) < 1:
        print( "Usage: python shader_compiler.py [--glsl] [-p PERM]... <shader_json_or_directory_or_shader_file>" )
        print( "" )
        print( "  Default: compiles HLSL shaders (.vs.hlsl, .ps.hlsl, .cs.hlsl) via DXC" )
        print( "  --glsl:  compiles GLSL shaders (.vert, .frag, .comp) via glslangValidator" )
        print( f"  -p/--perm: add a permutation ({', '.join( FLAG_MAP.keys() )})" )
        sys.exit( 1 )

    path = Path( args[ 0 ] )

    with open( LOG_FILE, "w" ) as log:
        mode_str = "GLSL (glslangValidator)" if "--glsl" in sys.argv else "HLSL (DXC)"
        log.write( f"Shader build {datetime.now()} [{mode_str}]\n" )
        log.write( "================================\n\n" )

        # Single shader mode - detected by file extension
        hlsl_single = path.name.endswith( ".vs.hlsl" ) or path.name.endswith( ".ps.hlsl" ) or path.name.endswith( ".cs.hlsl" )
        glsl_single = path.suffix in ( ".vert", ".frag", ".comp" )

        if hlsl_single or glsl_single:
            success = compile_single_shader_source( str( path.name ), tuple( cli_perms ), tuple( cli_macros ), log )
            summary = "\nAll shaders compiled successfully." if success else "\nBuild failed with 1 error(s)."
            print( summary )
            log.write( summary + "\n" )
            sys.exit( 0 if success else 1 )

        # Full build mode
        if path.is_dir():
            data = load_all_shaders( path )
        elif path.is_file():
            with open( path, "r" ) as f:
                data = json.load( f )
        else:
            print( f"ERROR: '{path}' is not a valid file or directory" )
            sys.exit( 1 )

        records = parse_shaders( data )
        records = deduplicate( records )

        errors  = sum( 1 for r in records if not compile_record( r, log ) )
        summary = f"\nBuild failed with {errors} error(s)." if errors else "\nAll shaders compiled successfully."
        print( summary )
        log.write( summary + "\n" )


if __name__ == "__main__":
    main()
