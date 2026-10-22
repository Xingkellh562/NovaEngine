#version 450 core

in vec3 outColor;
in float ao;
in float vDepth;
in float vDis;
out vec4 FragColor;

uniform sampler2DArray uTextures;
uniform float distance;

void main(){
	if(vDis < distance) discard;
	FragColor = texture(uTextures,outColor) * ao;
}