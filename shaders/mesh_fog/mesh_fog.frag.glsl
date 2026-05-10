#version 330 core

in struct fragment_data {
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
} fragment;

layout(location=0) out vec4 FragColor;

uniform sampler2D image_texture;
uniform mat4 view;
uniform vec3 light;
uniform vec3  light_color;
uniform float light_range;

uniform float fog_distance;
uniform vec3  fog_color;

struct phong_structure {
    float ambient;
    float diffuse;
    float specular;
    float specular_exponent;
};

struct texture_settings_structure {
    bool use_texture;
    bool texture_inverse_v;
    bool two_sided;
};

struct material_structure {
    vec3  color;
    float alpha;
    phong_structure phong;
    texture_settings_structure texture_settings;
};

uniform material_structure material;

void main()
{
    mat3 O = transpose(mat3(view));
    vec3 last_col = vec3(view * vec4(0.0, 0.0, 0.0, 1.0));
    vec3 camera_position = -O * last_col;

    vec3 N = normalize(fragment.normal);
    if (material.texture_settings.two_sided && gl_FrontFacing == false)
        N = -N;

    vec3 L_vec = light - fragment.position;
    float light_dist = length(L_vec);
    vec3 L = normalize(L_vec);
    float attenuation = clamp(1.0 - light_dist / light_range, 0.0, 1.0);

    float diffuse_component = max(dot(N, L), 0.0) * attenuation;

    float specular_component = 0.0;
    if (diffuse_component > 0.0) {
        vec3 R = reflect(-L, N);
        vec3 V = normalize(camera_position - fragment.position);
        specular_component = pow(max(dot(R, V), 0.0), material.phong.specular_exponent) * attenuation;
    }

    vec2 uv_image = fragment.uv;
    if (material.texture_settings.texture_inverse_v)
        uv_image.y = 1.0 - uv_image.y;

    vec4 color_image_texture = texture(image_texture, uv_image);
    if (!material.texture_settings.use_texture)
        color_image_texture = vec4(1.0);

    vec3 color_object = fragment.color * material.color * color_image_texture.rgb;

    float Ka = material.phong.ambient;
    float Kd = material.phong.diffuse;
    float Ks = material.phong.specular;
    vec3 color_shading = (Ka * color_object)
                       + (Kd * diffuse_component) * color_object * light_color
                       + Ks * specular_component * light_color;

    // Fog
    float d = length(fragment.position - camera_position);
    float alpha_fog = clamp(d / fog_distance, 0.0, 1.0);
    vec3 final_color = mix(color_shading, fog_color, alpha_fog);

    FragColor = vec4(final_color, material.alpha * color_image_texture.a);
}
