import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", 
                   "quad3_vert.vert", "quad3.vert", "taa.frag"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

    subprocess.run([glslang_cmd, "-V", "-DTAA_PASS_STATIC", "simple.vert", "-o", "simple_taa_static.vert.spv"])
    subprocess.run([glslang_cmd, "-V", "-DTAA_PASS_DYNAMIC", "simple.vert", "-o", "simple_taa_dynamic.vert.spv"])
    subprocess.run([glslang_cmd, "-V", "-DTAA_PASS_DYNAMIC", "simple_shadow.frag", "-o", "simple_shadow_taa_dynamic.frag.spv"])

