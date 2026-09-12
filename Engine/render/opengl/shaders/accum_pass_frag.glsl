#version 330 core

in vec3 uv;
in float viewDepth;

layout(location = 0) out vec4 outAccum;
layout(location = 1) out float outRevealage;

uniform sampler2DArray uTextureArray;

void main(){
	vec4 texColor = texture(uTextureArray,uv);
	float alpha = texColor.a;

	if(alpha < 0.01) discard;

	vec3 color = texColor.rgb;

	float linearDepth = abs(viewDepth);
	float w = alpha * max(1e-2,3e3 * pow(1.0 - clamp(linearDepth / 100.0,0.0,1.0),3.0));

	w = clamp(w,1e-2,3e3);
	outAccum = vec4(color * alpha * w,alpha * w);
	outRevealage = alpha;

	//outAccum = vec4(1.0, 0.0, 0.0, 1.0);   // 红色
    //outRevealage = 1.0;
}