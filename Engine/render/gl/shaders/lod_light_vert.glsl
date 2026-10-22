#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aUv;

out vec3 normal;
out vec3 vWorldPos;
out vec3 vUv;
out float vDis;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main(){
	vec4 worldPos = model * vec4(aPos, 1.0);
	vWorldPos = worldPos.xyz;

	vec4 viewPos = view * worldPos;
	vDis = length(viewPos.xyz);

	gl_Position = projection * viewPos;
	normal = normalMatrix * aNormal;
	vUv = aUv;
}