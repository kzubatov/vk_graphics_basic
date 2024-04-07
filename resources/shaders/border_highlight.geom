#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in GS_IN 
{
    vec2 texCoord;
} gsIn[];

layout(location = 0) out vec2 texCoord;
// layout(location = 1) noperspective out vec3 d;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        texCoord = gsIn[i].texCoord;
        gl_Position = gl_in[i].gl_Position;
        // d = vec3(0); d[i] = 1.0;
        EmitVertex();
    }
    EndPrimitive();
}