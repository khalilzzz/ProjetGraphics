#include "grass.hpp"
#include <cmath>
using namespace cgp;

void grass_structure::initialize(opengl_shader_structure const &shader,
                                 std::function<float(float, float)> height_fn)
{
    // Upright quad: from (−0.25, 0, 0) to (0.25, 0, 0.8)
    mesh m;
    m.position = {{-0.25f, 0, 0}, {0.25f, 0, 0}, {0.25f, 0, 0.8f}, {-0.25f, 0, 0.8f}};
    m.uv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    m.connectivity = {{0, 1, 2}, {0, 2, 3}};
    m.normal_update();
    m.fill_empty_field();

    quad.initialize_data_on_gpu(m);
    quad.texture.load_and_initialize_texture_2d_on_gpu(
        project::path + "assets/grass.png", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    quad.material.color = {1.0f, 1.0f, 1.0f};
    quad.material.phong.ambient = 0.5f;
    quad.material.phong.diffuse = 0.5f;
    quad.material.phong.specular = 0.0f;
    quad.material.texture_settings.two_sided = true;
    quad.shader = shader;

    // Place 1200 tufts (2 crossed quads each) from r=7 to r=20
    int count = 0;
    while (count < 3000)
    {
        float x = rand_uniform(-20.0f, 20.0f);
        float y = rand_uniform(-20.0f, 20.0f);
        float r = std::sqrt(x * x + y * y);
        if (r < 7.0f || r > 19.5f)
            continue;
        float z = height_fn(x, y);
        positions.push_back({x, y, z});
        angles.push_back(rand_uniform(0.0f, Pi));
        ++count;
    }
}

void grass_structure::draw(environment_structure const &env)
{
    for (int i = 0; i < (int)positions.size(); ++i)
    {
        float a = angles[i];
        for (int k = 0; k < 2; ++k)
        {
            quad.model.translation = positions[i];
            quad.model.rotation = rotation_transform::from_axis_angle({0, 0, 1}, a + k * Pi * 0.5f);
            cgp::draw(quad, env);
        }
    }
}
