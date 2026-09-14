#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 uv;
void main() {
    uv = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
    //const vec2 positions[6] = vec2[6](
    //    vec2(-1.0,  1.0),
    //    vec2(-1.0, -1.0),
    //    vec2( 1.0, -1.0),
    //    vec2(-1.0,  1.0),
    //    vec2( 1.0, -1.0),
    //    vec2( 1.0,  1.0)
    //);
    //gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}