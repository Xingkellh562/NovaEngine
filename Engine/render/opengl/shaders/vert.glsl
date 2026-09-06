#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aAo;

out vec3 outColor;
out float ao;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main(){
	gl_Position = projection * view * model * vec4(aPos,1.0);
	outColor = aColor;
	ao = aAo;
}