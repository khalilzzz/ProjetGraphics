#include "smoke.hpp"
#include <algorithm>
#include <cmath>
using namespace cgp;

void smoke_structure::initialize(vec3 const &emitter_pos)
{
    emitter = emitter_pos;
    particles.resize(200);

    mesh quad;
    quad.position = {{-0.5f, -0.5f, 0}, {0.5f, -0.5f, 0}, {0.5f, 0.5f, 0}, {-0.5f, 0.5f, 0}};
    quad.uv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    quad.connectivity = {{0, 1, 2}, {0, 2, 3}};
    quad.normal_update();
    quad.fill_empty_field();

    billboard.initialize_data_on_gpu(quad);
    billboard.texture.load_and_initialize_texture_2d_on_gpu(
        project::path + "assets/smoke.png", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    billboard.material.phong.ambient = 1.0f;
    billboard.material.phong.diffuse = 0.0f;
    billboard.material.phong.specular = 0.0f;
    billboard.material.color = {0.9f, 0.85f, 0.8f};
}

void smoke_structure::emit(float t)
{
    for (auto &p : particles)
    {
        if (p.alive)
            continue;
        p.p0 = emitter + vec3(rand_uniform(-0.2f, 0.2f), rand_uniform(-0.2f, 0.2f), 0.0f);
        p.t0 = t;
        p.lifetime = rand_uniform(4.0f, 7.0f);
        p.size_init = rand_uniform(0.5f, 1.2f);
        p.alive = true;
        p.current_pos = p.p0;
        p.current_size = p.size_init;
        p.current_alpha = 0.0f;
        return;
    }
}

void smoke_structure::update(float t, float dt)
{
    if (emission_rate > 0.0f)
    {
        time_accum += dt;
        float period = 1.0f / emission_rate;
        while (time_accum >= period)
        {
            emit(t);
            time_accum -= period;
        }
    }

    for (auto &p : particles)
    {
        if (!p.alive)
            continue;
        float age = t - p.t0;
        if (age < 0 || age > p.lifetime)
        {
            p.alive = false;
            continue;
        }

        float norm_age = age / p.lifetime;
        p.current_pos.x = p.p0.x + 0.4f * age * std::cos(1.5f * age);
        p.current_pos.y = p.p0.y + 0.4f * age * std::sin(1.5f * age);
        p.current_pos.z = p.p0.z + 1.8f * age;
        p.current_size = p.size_init * (1.0f + 1.2f * norm_age);
        // Fade in then out
        float fade_in = std::min(1.0f, age / 0.5f);
        float fade_out = 1.0f - norm_age;
        p.current_alpha = fade_in * fade_out * 0.55f;
    }
}

void smoke_structure::draw(environment_structure const &env, vec3 const &camera_pos, mat4 const &view)
{
    // Build billboard rotation from camera view matrix (row-major in CGP)
    vec3 cam_right = {view(0, 0), view(0, 1), view(0, 2)};
    vec3 cam_up = {view(1, 0), view(1, 1), view(1, 2)};
    vec3 cam_bwd = cross(cam_right, cam_up);
    mat3 R(
        {cam_right.x, cam_up.x, cam_bwd.x},
        {cam_right.y, cam_up.y, cam_bwd.y},
        {cam_right.z, cam_up.z, cam_bwd.z});
    rotation_transform billboard_rot = rotation_transform::from_matrix(R);

    // Collect alive particles and sort back-to-front
    std::vector<int> alive_idx;
    for (int i = 0; i < (int)particles.size(); ++i)
        if (particles[i].alive)
            alive_idx.push_back(i);

    std::sort(alive_idx.begin(), alive_idx.end(), [&](int a, int b)
              {
        float da = norm(particles[a].current_pos - camera_pos);
        float db = norm(particles[b].current_pos - camera_pos);
        return da > db; });

    for (int i : alive_idx)
    {
        auto &p = particles[i];
        billboard.model.translation = p.current_pos;
        billboard.model.rotation = billboard_rot;
        billboard.model.scaling = p.current_size;
        billboard.material.alpha = p.current_alpha;
        cgp::draw(billboard, env);
    }
}
