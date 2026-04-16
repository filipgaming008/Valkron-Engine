#version 460 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

struct PbrMaterial {
    vec3 albedoColor;
    float metallic;
    float roughness;
    float ao;

    int hasAlbedoMap;
    sampler2D albedoMap;

    int hasNormalMap;
    sampler2D normalMap;

    int hasMetallicMap;
    sampler2D metallicMap;

    int hasRoughnessMap;
    sampler2D roughnessMap;

    int hasAoMap;
    sampler2D aoMap;

    int hasAlphaMap;
    sampler2D alphaMap;
};

struct DirectionalLight {
    int enabled;
    vec3 direction;
    vec3 color;
    float intensity;
    float ambientStrength;
};

struct PointLight {
    int enabled;
    vec3 position;
    vec3 color;
    float intensity;
    float ambientStrength;
    float range;
};

uniform PbrMaterial u_PbrMaterial;
uniform int u_LightType;
uniform DirectionalLight u_DirectionalLight;
uniform PointLight u_PointLight;
uniform vec3 u_ViewPos;
uniform vec3 u_SelectionColor;
uniform float u_SelectionMix;
uniform float u_EditorExposure;

const float PI = 3.14159265359;

float distributionGGX(float NdotH, float roughness) {
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (NdotH * NdotH) * (alphaSq - 1.0) + 1.0;
    return alphaSq / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 resolveSurfaceNormal() {
    vec3 normal = normalize(fs_in.Normal);
    if (u_PbrMaterial.hasNormalMap == 1) {
        vec3 mappedNormal = texture(u_PbrMaterial.normalMap, fs_in.TexCoord).rgb;
        mappedNormal = normalize(mappedNormal * 2.0 - 1.0);
        normal = normalize(mix(normal, mappedNormal, 0.35));
    }

    return normal;
}

void main() {
    vec3 normal = resolveSurfaceNormal();
    vec3 viewDir = normalize(u_ViewPos - fs_in.FragPos);
    float alpha = 1.0;

    vec3 albedo = clamp(u_PbrMaterial.albedoColor, 0.0, 1.0);
    if (u_PbrMaterial.hasAlbedoMap == 1) {
        vec4 sampledAlbedo = texture(u_PbrMaterial.albedoMap, fs_in.TexCoord);
        albedo = pow(sampledAlbedo.rgb, vec3(2.2));
        alpha = sampledAlbedo.a;
    }

    if (u_PbrMaterial.hasAlphaMap == 1) {
        alpha *= clamp(texture(u_PbrMaterial.alphaMap, fs_in.TexCoord).r, 0.0, 1.0);
    }

    if (alpha < 0.1) {
        discard;
    }

    float metallic = clamp(u_PbrMaterial.metallic, 0.0, 1.0);
    if (u_PbrMaterial.hasMetallicMap == 1) {
        metallic = clamp(texture(u_PbrMaterial.metallicMap, fs_in.TexCoord).r, 0.0, 1.0);
    }

    float roughness = clamp(u_PbrMaterial.roughness, 0.04, 1.0);
    if (u_PbrMaterial.hasRoughnessMap == 1) {
        roughness = clamp(texture(u_PbrMaterial.roughnessMap, fs_in.TexCoord).r, 0.04, 1.0);
    }

    float ao = clamp(u_PbrMaterial.ao, 0.0, 1.0);
    if (u_PbrMaterial.hasAoMap == 1) {
        ao = clamp(texture(u_PbrMaterial.aoMap, fs_in.TexCoord).r, 0.0, 1.0);
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotL = 0.0;
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotH = 0.0;
    float VdotH = 0.0;

    vec3 lightDir = vec3(0.0, 1.0, 0.0);
    vec3 radiance = vec3(0.0);
    float ambientStrength = 0.0;

    if (u_LightType == 0 && u_DirectionalLight.enabled == 1) {
        lightDir = normalize(-u_DirectionalLight.direction);
        vec3 halfwayDir = normalize(viewDir + lightDir);
        NdotL = max(dot(normal, lightDir), 0.0);
        NdotH = max(dot(normal, halfwayDir), 0.0);
        VdotH = max(dot(viewDir, halfwayDir), 0.0);
        radiance = u_DirectionalLight.color * max(0.01, u_DirectionalLight.intensity);
        ambientStrength = clamp(u_DirectionalLight.ambientStrength, 0.0, 1.0);
    } else if (u_LightType == 1 && u_PointLight.enabled == 1) {
        vec3 toLight = u_PointLight.position - fs_in.FragPos;
        float lightDistance = length(toLight);
        lightDir = lightDistance > 0.0001 ? normalize(toLight) : vec3(0.0, 1.0, 0.0);
        vec3 halfwayDir = normalize(viewDir + lightDir);
        NdotL = max(dot(normal, lightDir), 0.0);
        NdotH = max(dot(normal, halfwayDir), 0.0);
        VdotH = max(dot(viewDir, halfwayDir), 0.0);

        float normalizedDistance = lightDistance / max(0.25, u_PointLight.range);
        float attenuation = 1.0 / (1.0 + normalizedDistance * normalizedDistance * 4.0);
        radiance = u_PointLight.color * max(0.01, u_PointLight.intensity) * attenuation;
        ambientStrength = clamp(u_PointLight.ambientStrength, 0.0, 1.0);
    }

    vec3 specular = vec3(0.0);
    vec3 diffuse = vec3(0.0);

    if (NdotL > 0.0) {
        float NDF = distributionGGX(NdotH, roughness);
        float G = geometrySmith(NdotV, NdotL, roughness);
        vec3 F = fresnelSchlick(VdotH, F0);

        vec3 numerator = NDF * G * F;
        float denominator = max(4.0 * NdotV * NdotL, 0.0001);
        specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        diffuse = kD * albedo / PI;
    }

    vec3 ambient = albedo * ambientStrength * ao;
    vec3 color = ambient + (diffuse + specular) * radiance * NdotL;

    color *= max(0.01, u_EditorExposure);
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    float highlightMix = clamp(u_SelectionMix, 0.0, 1.0);
    color = mix(color, u_SelectionColor, highlightMix);

    FragColor = vec4(color, alpha);
}
