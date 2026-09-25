#version 450 core
in vec2 uv;
out vec4 FragColor;

uniform sampler2D colorTex;
uniform sampler2D depthTex;
//uniform sampler2D shadowMap;
uniform sampler2DShadow shadowMap;

uniform sampler2D accumTex;
uniform sampler2D revealTex;

uniform mat4 invViewProj;
uniform mat4 lightSpaceMatrix;
uniform vec2 screenSize;

float ShadowCalculation(vec4 fragPosLightSpace){
    vec3 projCoords = fragPosLightSpace.xyz/ fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 0.0; // 超出范围，认为无阴影
    }

    float shadow = 0.0;
    vec2 texelSize = 1.0/textureSize(shadowMap,0);
    float bias = 0.001;
    int samples = 3;
    float radius = 1.5;
    for(int x = -samples/2;x <= samples/2;x++){
        for(int y = -samples/2;y <= samples/2;y++){
            vec2 offset = vec2(float(x),float(y)) * texelSize * radius;
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, projCoords.z - bias));
        }
    }
    //float bias = 0.001;
    //float shadow = 
    
    return 1 - shadow /(samples * samples);
    //return 1 - shadow;
}

void main() {
    vec2 texelIndex = floor(uv * 32.0);
    vec2 centerUV = (texelIndex + 0.5) / 32.0;

    //正常内容
    vec3 color = texture(colorTex,uv).rgb;
    float depth = texture(depthTex,uv).r;

    //将屏幕uv坐标，深度图结合得到ndc坐标，再通过逆矩阵转换回世界坐标
    vec4 ndc = vec4(uv * 2.0 - 1.0,depth * 2.0 - 1.0,1.0);
    vec4 worldPos = invViewProj * ndc;
    
    worldPos /= worldPos.w;

    vec4 centerWorldPos = vec4(floor(worldPos.xyz * 32.0)/32.0,1.0);

    vec4 lightSpacePos = lightSpaceMatrix * centerWorldPos;

    float shadowFactor = ShadowCalculation(lightSpacePos);

    vec4 accum = texture(accumTex, uv);
    float revealage = texture(revealTex, uv).r;

    vec3 finalColor;
    if (accum.a < 1e-5) {
        finalColor = color;
    }
    else{
        vec3 avgColor = accum.rgb / accum.a;
        finalColor = (avgColor * (1.0 - revealage) + color * revealage);
    }

    float shadowIntensity = 0.5;
    finalColor *= (1.0 - shadowIntensity * shadowFactor);

    FragColor = vec4(finalColor,1.0);

    //FragColor = vec4(accum.rgb/ accum.a, 1.0);
}