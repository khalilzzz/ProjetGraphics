#include "tree.hpp"
#include <cmath>
using namespace cgp;

void tree_structure::initialize(opengl_shader_structure const &shader,
                                std::function<float(float, float)> height_fn)
{
    /* tronc : un simple cylindre brun texture avec wood.jpg */
    trunk.initialize_data_on_gpu(mesh_primitive_cylinder(0.06f, {0, 0, 0}, {0, 0, 0.6f}, 8, 16, false));
    trunk.texture.load_and_initialize_texture_2d_on_gpu(
        project::path + "assets/wood.jpg", GL_REPEAT, GL_REPEAT);
    trunk.material.color = {0.45f, 0.28f, 0.12f};
    trunk.material.phong.ambient = 0.35f;
    trunk.material.phong.diffuse = 0.65f;
    trunk.material.phong.specular = 0.0f;
    trunk.shader = shader;

    /* feuillage : on empile trois cones de rayons decroissants pour obtenir une silhouette de conifere,
       concatenes dans une seule mesh pour ne faire qu'un appel draw par arbre */
    mesh leaves_mesh;
    leaves_mesh.push_back(mesh_primitive_cone(0.30f, 0.33f, {0, 0, 0.47f}, {0, 0, 1}, true, 14, 8));
    leaves_mesh.push_back(mesh_primitive_cone(0.23f, 0.30f, {0, 0, 0.67f}, {0, 0, 1}, true, 14, 8));
    leaves_mesh.push_back(mesh_primitive_cone(0.17f, 0.27f, {0, 0, 0.87f}, {0, 0, 1}, true, 14, 8));
    leaves.initialize_data_on_gpu(leaves_mesh);
    leaves.material.color = {0.18f, 0.45f, 0.12f};
    leaves.material.phong.ambient = 0.4f;
    leaves.material.phong.diffuse = 0.6f;
    leaves.material.phong.specular = 0.0f;
    leaves.shader = shader;

    /* placement de 400 arbres en couronne autour du volcan via rejection sampling sur un carre,
       avec rotation et echelle aleatoires pour casser l'effet de clones */
    int count = 0;
    while (count < 400)
    {
        float x = rand_uniform(-30.0f, 30.0f);
        float y = rand_uniform(-30.0f, 30.0f);
        float r = std::sqrt(x * x + y * y);
        if (r < 8.5f || r > 29.0f)
            continue;
        float z = height_fn(x, y);
        positions.push_back({x, y, z});
        float angle = rand_uniform(0.0f, 2.0f * Pi);
        rotations.push_back(rotation_transform::from_axis_angle({0, 0, 1}, angle));
        scalings.push_back(rand_uniform(0.8f, 1.5f));
        ++count;
    }
}

void tree_structure::draw(environment_structure const &env, bool wireframe)
{
    for (int i = 0; i < (int)positions.size(); ++i)
    {
        trunk.model.translation = positions[i];
        trunk.model.rotation = rotations[i];
        trunk.model.scaling = scalings[i];
        leaves.model.translation = positions[i];
        leaves.model.rotation = rotations[i];
        leaves.model.scaling = scalings[i];
        cgp::draw(trunk, env);
        cgp::draw(leaves, env);
        if (wireframe)
        {
            cgp::draw_wireframe(trunk, env);
            cgp::draw_wireframe(leaves, env);
        }
    }
}
