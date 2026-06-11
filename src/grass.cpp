#include "grass.hpp"
#include <cmath>
using namespace cgp;

void grass_structure::initialize(opengl_shader_structure const &shader,
                                 std::function<float(float, float)> height_fn)
{
    /* on construit a la main un quad vertical de 0.5 x 0.8, qui servira de billboard pour chaque touffe d'herbe */
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

    /* placement de 3000 touffes (chacune composee de 2 quads croises au dessin) en couronne autour du volcan,
       via rejection sampling sur un carre, avec un angle de base aleatoire pour briser la regularite */
    int count = 0;
    while (count < 3000)
    {
        float x = rand_uniform(-30.0f, 30.0f);
        float y = rand_uniform(-30.0f, 30.0f);
        float r = std::sqrt(x * x + y * y);
        if (r < 7.0f || r > 29.5f)
            continue;
        float z = height_fn(x, y);
        positions.push_back({x, y, z});
        angles.push_back(rand_uniform(0.0f, Pi));
        ++count;
    }
}

void grass_structure::draw(environment_structure const &env)
{
    /* pour chaque touffe, on dessine 2 quads tournes a 90 degres l'un de l'autre autour de Z :
       ainsi la touffe a du volume sous tous les angles, sans avoir a tourner les quads face camera */
    for (int i = 0; i < (int)positions.size(); ++i)
    {
        quad.model.translation = positions[i];
        float a = angles[i];
        for (int k = 0; k < 2; ++k)
        {
            quad.model.rotation = rotation_transform::from_axis_angle({0, 0, 1}, a + k * Pi * 0.5f);
            cgp::draw(quad, env);
        }
    }
}
