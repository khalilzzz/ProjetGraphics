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

uniform mat4 view;
uniform vec3  light;
uniform float light_range;
uniform vec3  light_color;
uniform float fog_distance;
uniform vec3  fog_color;
uniform vec3  camera_position;

/* tout le bloc bruit + FBM ci-dessous (hash, value noise, fbm 4 octaves)
   a ete genere par IA : ce sont des recettes GLSL classiques de bruit procedural */
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
    /* tout le corps de main() (distorsion UV animee, champ de chaleur, glow additif) a ete
       genere par IA puis adapte aux couleurs et a la vitesse de notre lave */
    float t = time * 0.25;
    vec2 uv = fragment.uv;

    /* on distord les UV par un FBM dependant du temps pour donner l'aspect de matiere qui bouillonne */
    float dx = fbm(uv * 3.0 + vec2(t, 0.0)) - 0.5;
    float dy = fbm(uv * 3.0 + vec2(5.2, 1.3 + t)) - 0.5;
    vec2 uv_anim = uv + 0.18 * vec2(dx, dy);

    /* champ de chaleur : un second FBM interpole entre rouge sombre et jaune-orange selon l'intensite */
    float heat = fbm(uv_anim * 4.0 + t);
    vec3 hot_color  = vec3(1.0, 0.85, 0.1);
    vec3 cool_color = vec3(0.8, 0.1, 0.02);
    vec3 lava = mix(cool_color, hot_color, heat * heat);

    /* glow additif sur les zones les plus chaudes pour accentuer la sensation d'incandescence */
    lava += 0.3 * vec3(1.0, 0.4, 0.0) * max(0.0, heat - 0.6);

    /* attenuation legere par la lumiere : on garde la lave emissive (elle reste brillante au loin) */
    vec3 L_vec = light - fragment.position;
    float light_dist = length(L_vec);
    float attenuation = clamp(1.0 - light_dist / light_range, 0.0, 1.0);
    lava *= (0.6 + 0.4 * attenuation);

    /* melange final avec le brouillard atmospherique selon la distance a la camera */
    float d = length(fragment.position - camera_position);
    float alpha_fog = clamp(d / fog_distance, 0.0, 1.0);
    vec3 final_color = mix(lava, fog_color, alpha_fog);

    FragColor = vec4(final_color, 1.0);
}
