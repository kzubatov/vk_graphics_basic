import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.vert", "quad.vert", "quad.frag", "simple_shadow.frag", 
                   "terrain_grid.vert", "terrain_grid.tesc", "terrain_grid.tese", "terrain.frag", "wireframe.geom",
                   "noise.frag", "quad3_vert.vert"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "-V", shader, "-o", "{}.spv".format(shader)])
    
    subprocess.run([glslang_cmd, "-V", "-DWIREFRAME", "terrain.frag", "-o", "terrain_wireframe.frag.spv"])

