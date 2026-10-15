#version 450 core

in vec3 uv;
out vec4 FragColor;

uniform sampler2DArray uTextureArray;

void main(){
	float alpha = texture(uTextureArray,uv).a;
	if(alpha <  0.99) discard;
	FragColor = vec4(1.0);
}