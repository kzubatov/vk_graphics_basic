import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["Gaussian_blur.comp", "simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", "simple_quad_3vert.vert", "simple_quad_3vert.frag"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

