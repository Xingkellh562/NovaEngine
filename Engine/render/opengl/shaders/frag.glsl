#version 330 core

in vec3 outColor;
in float ao;
in float vDepth;
out vec4 FragColor;

uniform sampler2DArray uTextures;

void main(){
	FragColor = texture(uTextures,outColor) * ao;
	//gl_FragDepth = vDepth;
	//FragColor =vec4(vec3(vDepth), 1.0);
}