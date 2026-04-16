#version 460 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

struct Material {
    int hasDiffuseMap;
    sampler2D diffuseMap;
    int hasSpecularMap;
    sampler2D specularMap;
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
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

uniform Material u_Material;
uniform int u_LightType;
uniform DirectionalLight u_DirectionalLight;
uniform PointLight u_PointLight;
uniform vec3 u_ViewPos;
uniform vec3 u_SelectionColor;
uniform float u_SelectionMix;
uniform float u_EditorExposure;
uniform int u_OverrideDiffuseMapEnabled;
uniform sampler2D u_OverrideDiffuseMap;
uniform int u_OverrideAlphaMapEnabled;
uniform sampler2D u_OverrideAlphaMap;

void main() {
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(u_ViewPos - fs_in.FragPos);
    float alpha = 1.0;

    vec3 baseColor = u_Material.diffuseColor;
    vec4 diffuseSample = vec4(baseColor, alpha);
    if (u_Material.hasDiffuseMap == 1) {
        diffuseSample = texture(u_Material.diffuseMap, fs_in.TexCoord);
    }

    if (u_OverrideDiffuseMapEnabled == 1) {
        diffuseSample = texture(u_OverrideDiffuseMap, fs_in.TexCoord);
    }

    baseColor = diffuseSample.rgb;
    alpha = diffuseSample.a;

    if (u_OverrideAlphaMapEnabled == 1) {
        alpha *= clamp(texture(u_OverrideAlphaMap, fs_in.TexCoord).r, 0.0, 1.0);
    }

    if (alpha < 0.1) {
        discard;
    }

    vec3 specularBase = u_Material.specularColor;
    if (u_Material.hasSpecularMap == 1) {
        specularBase = texture(u_Material.specularMap, fs_in.TexCoord).rgb;
    }

    vec3 shadedColor = baseColor * 0.22;

    if (u_LightType == 0 && u_DirectionalLight.enabled == 1) {
        vec3 lightDir = normalize(-u_DirectionalLight.direction);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float diff = max(dot(normal, lightDir), 0.0);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), max(1.0, u_Material.shininess));

        vec3 ambient = baseColor * u_DirectionalLight.color * clamp(u_DirectionalLight.ambientStrength, 0.01, 1.0);
        vec3 diffuse = diff * baseColor * u_DirectionalLight.color * max(0.01, u_DirectionalLight.intensity);
        vec3 specular = spec * specularBase * u_DirectionalLight.color * max(0.10, u_DirectionalLight.intensity);
        shadedColor = ambient + diffuse + specular;
    } else if (u_LightType == 1 && u_PointLight.enabled == 1) {
        vec3 toLight = u_PointLight.position - fs_in.FragPos;
        float lightDistance = length(toLight);
        vec3 lightDir = lightDistance > 0.0001 ? normalize(toLight) : vec3(0.0, 1.0, 0.0);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float normalizedDistance = lightDistance / max(0.25, u_PointLight.range);
        float attenuation = 1.0 / (1.0 + normalizedDistance * normalizedDistance * 4.0);
        float diff = max(dot(normal, lightDir), 0.0);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), max(1.0, u_Material.shininess));

        vec3 ambient = baseColor * u_PointLight.color * clamp(u_PointLight.ambientStrength, 0.01, 1.0);
        vec3 diffuse = diff * baseColor * u_PointLight.color * max(0.01, u_PointLight.intensity) * attenuation;
        vec3 specular = spec * specularBase * u_PointLight.color * max(0.10, u_PointLight.intensity) * attenuation;
        shadedColor = ambient + diffuse + specular;
    }

    shadedColor *= max(0.01, u_EditorExposure);

    float highlightMix = clamp(u_SelectionMix, 0.0, 1.0);
    shadedColor = mix(shadedColor, u_SelectionColor, highlightMix);

    FragColor = vec4(shadedColor, alpha);
}
