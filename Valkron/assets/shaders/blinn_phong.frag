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

struct Light {
    vec3 position;
    vec3 color;
    vec3 ambient;
};

const int MAX_LIGHTS = 8;

uniform Material u_Material;
uniform int u_LightCount;
uniform Light u_Lights[MAX_LIGHTS];
uniform vec3 u_ViewPos;
uniform vec3 u_SelectionColor;
uniform float u_SelectionMix;

void main() {
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(u_ViewPos - fs_in.FragPos);

    vec3 baseColor = u_Material.diffuseColor;
    if (u_Material.hasDiffuseMap == 1) {
        baseColor = texture(u_Material.diffuseMap, fs_in.TexCoord).rgb;
    }

    vec3 specularBase = u_Material.specularColor;
    if (u_Material.hasSpecularMap == 1) {
        specularBase = texture(u_Material.specularMap, fs_in.TexCoord).rgb;
    }

    int lightCount = clamp(u_LightCount, 0, MAX_LIGHTS);
    vec3 ambient = vec3(0.0);
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < lightCount; ++i) {
        vec3 lightDir = normalize(u_Lights[i].position - fs_in.FragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float diff = max(dot(normal, lightDir), 0.0);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), max(1.0, u_Material.shininess));

        ambient += u_Lights[i].ambient * baseColor;
        diffuse += diff * baseColor * u_Lights[i].color;
        specular += spec * specularBase * u_Lights[i].color;
    }

    if (lightCount == 0) {
        ambient = vec3(0.18) * baseColor;
    }

    vec3 shadedColor = ambient + diffuse + specular;
    float highlightMix = clamp(u_SelectionMix, 0.0, 1.0);
    shadedColor = mix(shadedColor, u_SelectionColor, highlightMix);

    FragColor = vec4(shadedColor, 1.0);
}
