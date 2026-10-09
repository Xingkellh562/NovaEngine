#version 450 core

in vec3 normal;
in vec3 vWorldPos;

//平行光光照方向
uniform vec3 lightDir;

layout(location = 0) out vec3 lightCoe;

void main(){
	vec3 N = normalize(normal);
	vec3 L = normalize(-lightDir);
    vec3 V = normalize(-vWorldPos);    // 指向摄像机

    //漫反射
    float diff = max(dot(N, L) * 0.5 + 0.3,0.0);

    // ---------- Blinn-Phong 镜面反射（单色高光） ----------
    vec3 H = normalize(-L + V);                  // 半程向量
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // 背光面不应该有高光，乘上 step 消除漏光
    spec *= step(0.0, dot(N, -L));

    // ---------- 合并成单通道系数 ----------
    lightCoe = vec3(diff * 1.0 ,spec * 0.2,0.0);
}