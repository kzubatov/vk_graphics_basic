import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", 
                   "quad3_vert.vert", "quad3.vert", "velocity.vert", "velocity.frag", "taa.frag"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

    subprocess.run([glslang_cmd, "-V", "-DUSE_JITTERING", "simple.vert", "-o", "simple_jitter.vert.spv"])

