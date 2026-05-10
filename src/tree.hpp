#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"
#include <functional>

struct tree_structure {
    cgp::mesh_drawable trunk;
    cgp::mesh_drawable leaves;

    std::vector<cgp::vec3>              positions;
    std::vector<cgp::rotation_transform> rotations;
    std::vector<float>                  scalings;

    void initialize(cgp::opengl_shader_structure const& shader,
                    std::function<float(float,float)> height_fn);
    void draw(environment_structure const& env, bool wireframe = false);
};
