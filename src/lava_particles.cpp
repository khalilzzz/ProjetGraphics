#include "lava_particles.hpp"
using namespace cgp;

static const vec3 GRAVITY = {0, 0, -9.81f};

void lava_particles_structure::initialize(vec3 const& emitter_pos)
{
    emitter = emitter_pos;
    particles.resize(800);

    sphere.initialize_data_on_gpu(mesh_primitive_sphere(1.0f));
    sphere.texture.load_and_initialize_texture_2d_on_gpu(
        project::path + "assets/lava.png", GL_REPEAT, GL_REPEAT);
    sphere.material.color          = {1.0f, 0.55f, 0.1f};
    sphere.material.phong.ambient  = 0.95f;
    sphere.material.phong.diffuse  = 0.05f;
    sphere.material.phong.specular = 0.0f;
}

void lava_particles_structure::emit(float t)
{
    for (auto& p : particles) {
        if (p.alive) continue;
        p.p0  = emitter + vec3(rand_uniform(-0.3f, 0.3f), rand_uniform(-0.3f, 0.3f), 0.0f);
        float speed = rand_uniform(7.0f, 10.5f) * velocity_scale;
        p.v0  = {rand_uniform(-2.2f, 2.2f), rand_uniform(-2.2f, 2.2f), speed};
        p.t0  = t;
        p.lifetime = rand_uniform(2.5f, 4.5f);
        p.size     = rand_uniform(0.07f, 0.18f);
        p.alive    = true;
        p.current_pos = p.p0;
        return;
    }
}

void lava_particles_structure::update(float t, float dt)
{
    if (emission_rate > 0.0f) {
        time_accum += dt;
        float period = 1.0f / emission_rate;
        while (time_accum >= period) {
            emit(t);
            time_accum -= period;
        }
    }

    for (auto& p : particles) {
        if (!p.alive) continue;
        float age = t - p.t0;
        if (age < 0 || age > p.lifetime) { p.alive = false; continue; }
        p.current_pos = 0.5f * GRAVITY * age * age + p.v0 * age + p.p0;
        if (p.current_pos.z < -4.0f) p.alive = false;
    }
}

void lava_particles_structure::draw(environment_structure const& env)
{
    for (auto& p : particles) {
        if (!p.alive) continue;
        sphere.model.translation = p.current_pos;
        sphere.model.scaling     = p.size;
        cgp::draw(sphere, env);
    }
}

void lava_particles_structure::mega_eruption(float t)
{
    int count = 0;
    for (auto& p : particles) {
        if (p.alive || count >= 450) continue;
        p.p0  = emitter + vec3(rand_uniform(-0.4f, 0.4f), rand_uniform(-0.4f, 0.4f), 0.0f);
        float speed  = rand_uniform(11.0f, 16.0f);
        float angle  = rand_uniform(0.0f, 2.0f * Pi);
        float spread = rand_uniform(0.0f, 4.0f);
        p.v0       = {spread * std::cos(angle), spread * std::sin(angle), speed};
        p.t0       = t;
        p.lifetime = rand_uniform(3.5f, 7.0f);
        p.size     = rand_uniform(0.1f, 0.25f);
        p.alive    = true;
        p.current_pos = p.p0;
        ++count;
    }
}
