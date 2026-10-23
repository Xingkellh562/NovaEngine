#version 450 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;

out vec2 uv;
out vec4 color;

uniform mat4 projection;

void main(){
	gl_Position = projection * vec4(aPos,0.0,1.0);
	uv = aUv;
	color = aColor;
}
