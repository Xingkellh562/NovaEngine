#version 330 core

in vec3 outColor;
out vec4 FragColor;

uniform sampler2DArray uTextures;

void main(){
	FragColor = texture(uTextures,outColor);
}