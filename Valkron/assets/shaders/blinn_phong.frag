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
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 color;
    vec3 ambient;
};

uniform Material u_Material;
uniform Light u_Light;
uniform vec3 u_ViewPos;
uniform vec3 u_SelectionColor;
uniform float u_SelectionMix;

void main() {
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(u_Light.position - fs_in.FragPos);
    vec3 viewDir = normalize(u_ViewPos - fs_in.FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    vec3 baseColor = u_Material.diffuseColor;
    if (u_Material.hasDiffuseMap == 1) {
        baseColor = texture(u_Material.diffuseMap, fs_in.TexCoord).rgb;
    }

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(1.0, u_Material.shininess));

    vec3 ambient = u_Light.ambient * baseColor;
    vec3 diffuse = diff * baseColor * u_Light.color;
    vec3 specular = spec * u_Material.specularColor * u_Light.color;

    vec3 shadedColor = ambient + diffuse + specular;
    float highlightMix = clamp(u_SelectionMix, 0.0, 1.0);
    shadedColor = mix(shadedColor, u_SelectionColor, highlightMix);

    FragColor = vec4(shadedColor, 1.0);
}
