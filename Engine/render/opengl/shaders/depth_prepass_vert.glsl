#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 uv;

void main(){
	uv = aUV;
	gl_Position = uProj * uView * uModel * vec4(aPos,1.0);
}