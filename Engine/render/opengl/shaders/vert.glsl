#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aAo;

out vec3 outColor;
out float ao;
out float vDepth;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main(){
	vec4 pos = projection * view * model * vec4(aPos,1.0);
	gl_Position = pos;
	vDepth = pos.z;
	outColor = aColor;
	ao = aAo;
}