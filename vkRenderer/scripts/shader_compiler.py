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
# Claude assisted writing this so there might be some weirdness, but looks reasonable. I don't write enough python to tell


# Config
GLSLANG    = r"C:\VulkanSDK\1.3.261.0\Bin\glslangValidator.exe"
SHADER_DIR = "..\\shaders\\"
OUT_DIR    = "..\\shaders_bin\\"
LOG_FILE   = "shader_build.log"

FLAG_MAP = {
    "msaa"    : "USE_MSAA",
    "skycube" : "USE_CUBE_SAMPLER",
}

TYPE_SUFFIX = {
    "vert" : "VS",
    "frag" : "PS",
    "comp" : "CS",
}

# Map from JSON key to file extension
TYPE_EXT = {
    "vs" : "vert",
    "ps" : "frag",
    "cs" : "comp",
}


@dataclass( frozen=True )
class ShaderRecord:
    source : str
    output : str
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
    for ext in ( ".vert", ".frag", ".comp" ):
        if name.endswith( ext ):
            return name[ :-len( ext ) ]
    return name

# Produce single file records for each shader type
def parse_shaders( data ) -> list[ ShaderRecord ]:
    records = []
    for shader in data[ "shaders" ]:
        attribs = shader.keys()

        sources = []
        for key in ( "vs", "ps", "cs" ):
            if key in attribs:
                sources.append( ( strip_shader_ext( shader[ key ] ), TYPE_EXT[ key ] ) )

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
            type_suffix = TYPE_SUFFIX.get( ext, "" )
            source      = f"{stem}.{ext}"
            output      = f"{OUT_DIR}{stem}{type_suffix}{perm_suffix}.spv"

            records.append( ShaderRecord(
                source = SHADER_DIR + source,
                output = output,
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
    cmd = [ GLSLANG, "-l", "-V", record.source, "-o", record.output, "-g" ]
    for macro in record.macros:
        cmd += [ "--define-macro", macro ]
    return cmd


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

    if len( sys.argv ) < 2:
        print( "Usage: python build_shaders.py <shader_json>" )
        sys.exit( 1 )
        
    path = Path( sys.argv[ 1 ] )

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

    with open( LOG_FILE, "w" ) as log:
        log.write( f"Shader build {datetime.now()}\n" )
        log.write( "================================\n\n" )

        errors = sum( 1 for r in records if not compile_record( r, log ) )

        summary = f"\nBuild failed with {errors} error(s)." if errors else "\nAll shaders compiled successfully."
        print( summary )
        log.write( summary + "\n" )


if __name__ == "__main__":
    main()