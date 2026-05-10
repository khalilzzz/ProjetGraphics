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
    vec4 color_image_texture = texture(image_texture, fragment.uv);
    if (color_image_texture.a < 0.4)
        discard;

    mat3 O = transpose(mat3(view));
    vec3 last_col = vec3(view * vec4(0.0, 0.0, 0.0, 1.0));
    vec3 camera_position = -O * last_col;

    vec3 N = normalize(fragment.normal);
    if (gl_FrontFacing == false) N = -N;

    vec3 L = normalize(light - fragment.position);
    float diffuse_component = max(dot(N, L), 0.0);

    vec3 color_object = fragment.color * material.color * color_image_texture.rgb;
    float Ka = material.phong.ambient;
    float Kd = material.phong.diffuse;
    vec3 color_shading = (Ka + Kd * diffuse_component) * color_object;

    FragColor = vec4(color_shading, 1.0);
}
