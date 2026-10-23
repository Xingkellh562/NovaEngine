#version 450 core

in vec2 uv;
in vec4 color;

out vec4 FragColor;

void main(){
	FragColor = color;
}