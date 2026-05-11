#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"

struct lava_particle {
    cgp::vec3 p0, v0, current_pos;
    float t0       = 0;
    float lifetime = 0;
    float size     = 0.1f;
    bool  alive    = false;
};

struct lava_particles_structure {
    std::vector<lava_particle> particles;
    cgp::mesh_drawable sphere;
    cgp::vec3 emitter;

    float emission_rate   = 60.0f;  // particles / second
    float velocity_scale  = 1.0f;   // multiplier on initial upward speed
    float time_accum      = 0.0f;

    void initialize(cgp::vec3 const& emitter_pos, cgp::opengl_shader_structure const& shader);
    void emit(float t);
    void update(float t, float dt);
    void draw(environment_structure const& env);
    void mega_eruption(float t);
};
