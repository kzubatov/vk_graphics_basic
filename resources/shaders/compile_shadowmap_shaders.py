import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", 
                   "quad3_vert.vert", "noise.frag", "quad_template.vert",
                   "simple_tessellation_control.tesc", "simple_tessellation_evaluation.tese"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])

