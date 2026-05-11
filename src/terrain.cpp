#include "terrain.hpp"
#include <cmath>
using namespace cgp;

float terrain_structure::evaluate_height(float x, float y) const
{
    float r = std::sqrt(x * x + y * y);
    float hv = 6.0f * std::exp(-r * r / (2.0f * 5.0f * 5.0f));
    float hp = 1.2f * noise_perlin({x * perlin_frequency, y * perlin_frequency},
                                   perlin_octaves, perlin_persistence);
    float hc = -1.5f * std::exp(-r * r / (2.0f * 1.2f * 1.2f));
    return hv + hp + hc;
}

void terrain_structure::initialize(opengl_shader_structure const &shader)
{
    saved_shader = shader;

    int N = grid_size;
    float L = terrain_length / 2.0f;

    mesh m = mesh_primitive_grid({-L, -L, 0}, {L, -L, 0}, {L, L, 0}, {-L, L, 0}, N, N);

    for (int i = 0; i < (int)m.position.size(); ++i)
    {
        vec3 &p = m.position[i];
        p.z = evaluate_height(p.x, p.y);
        float u = (p.x + L) / terrain_length;
        float v = (p.y + L) / terrain_length;
        m.uv[i] = {u * 6.0f, v * 6.0f};
    }
    m.normal_update();

    drawable.initialize_data_on_gpu(m);
    drawable.texture.load_and_initialize_texture_2d_on_gpu(
        project::path + "assets/rock3.png", GL_REPEAT, GL_REPEAT);
    drawable.material.color = {1.0f, 1.0f, 1.0f};
    drawable.material.phong.ambient = 0.4f;
    drawable.material.phong.diffuse = 0.6f;
    drawable.material.phong.specular = 0.05f;
    drawable.shader = saved_shader;
}

void terrain_structure::rebuild()
{
    // Preserve GPU state that initialize_data_on_gpu would reset
    auto saved_texture = drawable.texture;
    auto saved_material = drawable.material;

    int N = grid_size;
    float L = terrain_length / 2.0f;

    mesh m = mesh_primitive_grid({-L, -L, 0}, {L, -L, 0}, {L, L, 0}, {-L, L, 0}, N, N);

    for (int i = 0; i < (int)m.position.size(); ++i)
    {
        vec3 &p = m.position[i];
        p.z = evaluate_height(p.x, p.y);
        float u = (p.x + L) / terrain_length;
        float v = (p.y + L) / terrain_length;
        m.uv[i] = {u * 6.0f, v * 6.0f};
    }
    m.normal_update();

    drawable.clear();
    drawable.initialize_data_on_gpu(m);
    drawable.texture = saved_texture;
    drawable.material = saved_material;
    drawable.shader = saved_shader;
}

void terrain_structure::draw(environment_structure const &env, bool wireframe)
{
    cgp::draw(drawable, env);
    if (wireframe)
        cgp::draw_wireframe(drawable, env);
}
