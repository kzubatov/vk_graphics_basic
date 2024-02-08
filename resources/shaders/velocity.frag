#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 out_velocity;

layout(location = 0) in FS_IN
{
    vec4 prevPos;
} fIn;

void main()
{
    out_velocity = vec2(fIn.prevPos.xy / fIn.prevPos.w) * 0.5f + 0.5f;
}