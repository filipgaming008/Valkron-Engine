#version 460 core
out vec4 FragColor;

in vec3 v_Color;

uniform vec3 u_SelectionColor;
uniform float u_SelectionMix;
uniform float u_EditorExposure;

void main() {
    vec3 color = mix(v_Color, u_SelectionColor, clamp(u_SelectionMix, 0.0, 1.0));
    color *= max(0.01, u_EditorExposure);
    FragColor = vec4(color, 1.0);
}
