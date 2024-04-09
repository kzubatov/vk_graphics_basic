#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
// layout(line_strip, max_vertices = 6) out;

layout(location = 0) in vec2 texCoord_in[];
// layout(location = 1) in vec3 wNorm_in[];
layout(location = 2) in vec3 wPos_in[];
layout(location = 0) out vec2 texCoord;
// layout(location = 1) out vec3 wNorm;
layout(location = 2) out vec3 wPos;
layout(location = 3) noperspective out vec3 dist;

// layout(push_constant) uniform push_const
// {
//     mat4 mViewWorld;
//     mat4 mProj;
// };

void main()
{
    // texCoord = vec2(1);
    // wNorm = vec3(0);
    // dist = vec3(0);

    // gl_Position = mProj * mViewWorld * gl_in[0].gl_Position;
    // EmitVertex();
    // gl_Position = mProj * mViewWorld * (gl_in[0].gl_Position + 0.1 * vec4(wNorm_in[0], 0));
    // EmitVertex();
    // EndPrimitive();
    // gl_Position = mProj * mViewWorld * gl_in[1].gl_Position;
    // EmitVertex();
    // gl_Position = mProj * mViewWorld * (gl_in[1].gl_Position + 0.1 * vec4(wNorm_in[1], 0));
    // EmitVertex();
    // EndPrimitive();
    // gl_Position = mProj * mViewWorld * gl_in[2].gl_Position;
    // EmitVertex();
    // gl_Position = mProj * mViewWorld * (gl_in[2].gl_Position + 0.1 * vec4(wNorm_in[2], 0));
    // EmitVertex();
    // EndPrimitive();
    // wireframe code
    vec3 viewportPos0 = gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w;
    vec3 viewportPos1 = gl_in[1].gl_Position.xyz / gl_in[1].gl_Position.w;
    vec3 viewportPos2 = gl_in[2].gl_Position.xyz / gl_in[2].gl_Position.w;
    
    float s = length(cross(viewportPos0 - viewportPos1, viewportPos2 - viewportPos1));
    
    gl_Position = gl_in[0].gl_Position;
    texCoord = texCoord_in[0];
    // wNorm = wNorm_in[0];
    wPos = wPos_in[0];
    dist = vec3(0, s / distance(viewportPos1, viewportPos2), 0);
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    texCoord = texCoord_in[1];
    // wNorm = wNorm_in[1];
    wPos = wPos_in[0];
    dist = vec3(0, 0, s / distance(viewportPos0, viewportPos2));
    EmitVertex();
    
    gl_Position = gl_in[2].gl_Position;
    texCoord = texCoord_in[2];
    // wNorm = wNorm_in[2];
    wPos = wPos_in[0];
    dist = vec3(s / distance(viewportPos1, viewportPos0), 0, 0);
    EmitVertex();

    EndPrimitive();
}