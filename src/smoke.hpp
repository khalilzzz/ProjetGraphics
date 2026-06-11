#pragma once
#include "cgp/cgp.hpp"
#include "environment.hpp"

struct smoke_particle {
    cgp::vec3 p0;
    float t0       = 0;
    float lifetime = 0;
    float size_init = 0.5f;
    bool  alive     = false;

    cgp::vec3 current_pos;
    float     current_size  = 0;
    float     current_alpha = 0;
};

struct smoke_structure {
    std::vector<smoke_particle> particles;
    cgp::mesh_drawable billboard;
    cgp::vec3 emitter;

    float emission_rate = 4.0f;
    float time_accum    = 0.0f;

    /* buffer de travail pour le tri back-to-front des particules vivantes, garde en membre
       de la structure pour eviter une allocation de std::vector a chaque frame */
    std::vector<int> alive_idx;

    void initialize(cgp::vec3 const& emitter_pos);
    void emit(float t);
    void update(float t, float dt);
    void draw(environment_structure const& env, cgp::vec3 const& camera_pos, cgp::mat4 const& view);
};
