#version 330 core

in struct fragment_data {
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
} fragment;

layout(location=0) out vec4 FragColor;

uniform sampler2D image_texture;
uniform float time;

// Value noise
float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash2(i),               hash2(i + vec2(1, 0)), u.x),
        mix(hash2(i + vec2(0, 1)),  hash2(i + vec2(1, 1)), u.x),
        u.y
    );
}

float fbm(vec2 p) {
    float v = 0.0; float a = 0.5;
    for (int i = 0; i < 4; i++) { v += a * vnoise(p); p *= 2.0; a *= 0.5; }
    return v;
}

void main()
{
    float t = time * 0.25;
    vec2 uv = fragment.uv;

    // Distort UV with animated FBM
    float dx = fbm(uv * 3.0 + vec2(t, 0.0)) - 0.5;
    float dy = fbm(uv * 3.0 + vec2(5.2, 1.3 + t)) - 0.5;
    vec2 uv_anim = uv + 0.18 * vec2(dx, dy);

    // Lava colors: hot orange-red gradient based on noise
    float heat = fbm(uv_anim * 4.0 + t);
    vec3 hot_color  = vec3(1.0, 0.85, 0.1);   // bright yellow-orange
    vec3 cool_color = vec3(0.8, 0.1, 0.02);   // dark red
    vec3 lava = mix(cool_color, hot_color, heat * heat);

    // Additive bright glow at the hottest points
    lava += 0.3 * vec3(1.0, 0.4, 0.0) * max(0.0, heat - 0.6);

    FragColor = vec4(lava, 1.0);
}
