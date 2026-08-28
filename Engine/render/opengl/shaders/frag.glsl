#version 460 core

in vec3 outColor;
out vec4 FragColor;

uniform sampler2D uTexture;

void main(){
	FragColor = texture(uTexture,vec2(outColor.x,outColor.y));
}