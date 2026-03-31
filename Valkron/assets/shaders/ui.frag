#version 460 core

in vec2 v_TexCoord;
in vec4 v_Color;
in float v_SdfMode;

out vec4 FragColor;

uniform sampler2D u_FontAtlas;

void main() {
    if (v_SdfMode < 0.5) {
        FragColor = v_Color;
        return;
    }

    float alpha = texture(u_FontAtlas, v_TexCoord).r;
    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
}
