#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 normal;
out vec3 vWorldPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main(){
	vec4 worldPos = model * vec4(aPos, 1.0);
	vWorldPos = worldPos.xyz;

	gl_Position = projection * view * worldPos;
	normal = normalMatrix * aNormal;
}
