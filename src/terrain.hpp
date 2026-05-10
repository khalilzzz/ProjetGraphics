#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"

struct terrain_structure {
    cgp::mesh_drawable drawable;
    float terrain_length = 40.0f;
    int   grid_size      = 200;

    // Perlin noise parameters (editable at runtime)
    float perlin_frequency   = 0.15f;
    int   perlin_octaves     = 5;
    float perlin_persistence = 0.45f;

    float evaluate_height(float x, float y) const;
    void  initialize(cgp::opengl_shader_structure const& shader);
    void  rebuild();   // regenerate mesh with current Perlin params
    void  draw(environment_structure const& env, bool wireframe = false);

private:
    cgp::opengl_shader_structure saved_shader;
};
