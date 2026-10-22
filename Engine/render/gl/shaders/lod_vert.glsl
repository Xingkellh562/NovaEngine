#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aAo;

out vec3 outColor;
out float ao;
out float vDepth;
out float vDis;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main(){
	vec4 viewPos = view * model * vec4(aPos, 1.0);
    vec4 pos     = projection * viewPos;

    gl_Position = pos;

    // 视图空间原点是摄像机 → 长度即到摄像机的距离
    vDis = length(viewPos.xyz);

    vDepth = pos.z;

    outColor = aColor;
    ao = aAo;
}