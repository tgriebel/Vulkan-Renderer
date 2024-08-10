import os
import json
import subprocess

os.chdir("C:\\Users\\thoma\\source\\repos\\VulkanRenderer-main\\Vulkan-Renderer\\vkRenderer")

compilerPath = "C:\\VulkanSDK\\1.3.261.0\\Bin\\glslangValidator.exe "
shaderDir = "shaders\\"
outDir = "shaders_bin\\"

f = open ('single_shader.json', "r")
j_string = f.read()
f.close()
#print(j_string)

data = json.loads(j_string)

compilerCommands = []

for shaderElement in data['shaders']:
    attribs = shaderElement.keys()

    print(shaderElement)
    print(attribs)

    macros = ""
    shaders = []

    if 'vs' in attribs:
        shaders.append(shaderElement['vs'])
    if 'ps' in attribs:
        shaders.append(shaderElement['ps'])
    if 'cs' in attribs:
        shaders.append(shaderElement['cs'])

    macroSuffix = ""
    if 'perm' in attribs:
        perm = shaderElement['perm']

        if "msaa" in perm:
            macroSuffix += "_msaa"
            macros += " USE_MSAA"
        if "skycube" in perm:
            macroSuffix += "_skycube"
            macros += " USE_CUBE_SAMPLER"

    for shader in shaders:
        outFileName = outDir
        if "vert" in shader:
            outFileName += shader.removesuffix(".vert") + "VS"
        if "frag" in shader:
            outFileName += shader.removesuffix(".frag") + "PS"
        if "comp" in shader:
            outFileName += shader.removesuffix(".comp") + "CS"
        outFileName += macroSuffix
        outFileName += ".spv"

        args = " -g"
        if macros != "":
            args += " --define-macro" + macros

        compilerCommands.append("-l -V " + shaderDir + shader + " -o " + outFileName + args)

compilerCommands = set(compilerCommands)

for command in compilerCommands:
    subprocessCmd = compilerPath + command
    print(subprocessCmd)
    #subprocess.run(subprocessCmd, shell=True, check=True)