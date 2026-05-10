#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"
#include <functional>

struct grass_structure {
    cgp::mesh_drawable quad;
    std::vector<cgp::vec3>  positions;
    std::vector<float>      angles;

    void initialize(cgp::opengl_shader_structure const& shader,
                    std::function<float(float,float)> height_fn);
    void draw(environment_structure const& env);
};
