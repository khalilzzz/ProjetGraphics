#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"

struct terrain_structure {
    cgp::mesh_drawable drawable;
    float terrain_length = 60.0f;
    int   grid_size      = 280;

    /* parametres du Perlin du terrain, ajustables a la volee depuis la GUI */
    float perlin_frequency   = 0.15f;
    int   perlin_octaves     = 5;
    float perlin_persistence = 0.45f;

    float evaluate_height(float x, float y) const;
    void  initialize(cgp::opengl_shader_structure const& shader);
    void  rebuild();   /* regenere le maillage avec les parametres Perlin courants */
    void  draw(environment_structure const& env, bool wireframe = false);

private:
    cgp::opengl_shader_structure saved_shader;
    cgp::mesh build_mesh() const;
};
