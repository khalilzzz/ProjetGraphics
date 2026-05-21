#include "scene.hpp"
#include <cmath>

using namespace cgp;

// ============================================================
// INITIALIZE
// ============================================================
void scene_structure::initialize()
{
    std::cout << "[Volcano] Initializing scene...\n";

    // Camera
    camera_control.initialize(inputs, window);
    camera_control.set_rotation_axis_z();
    camera_control.look_at({20.0f, -12.0f, 12.0f}, {0, 0, 3}, {0, 0, 1});

    camera_projection = camera_projection_perspective{
        50.0f * Pi / 180.0f, 1.0f, 0.05f, 500.0f};

    // Background matches fog color
    environment.background_color = fog_color;

    global_frame.initialize_data_on_gpu(mesh_primitive_frame());

    // ---- Load shaders ----
    shader_fog.load(project::path + "shaders/mesh_fog/mesh_fog.vert.glsl",
                    project::path + "shaders/mesh_fog/mesh_fog.frag.glsl");

    shader_lava.load(project::path + "shaders/lava/lava.vert.glsl",
                     project::path + "shaders/lava/lava.frag.glsl");

    shader_grass.load(project::path + "shaders/grass/grass.vert.glsl",
                      project::path + "shaders/grass/grass.frag.glsl");

    // ---- Terrain ----
    terrain.initialize(shader_fog);

    // Find crater summit (approximately at r=0)
    float crater_z = terrain.evaluate_height(0.0f, 0.0f);
    vec3 crater_pos = {0.0f, 0.0f, crater_z};
    std::cout << "[Volcano] Crater z = " << crater_z << "\n";

    // ---- Lava pool at crater ----
    // Multi-ring grid so each vertex samples the real terrain height — avoids
    // the lava mesh floating above the volcano where Perlin bumps the terrain.
    {
        int N = 60;
        std::vector<float> radii = {0.0f, 0.55f, 1.1f, 1.65f, 2.2f};
        float offset = 0.12f; // push above terrain to avoid z-fighting
        mesh m;

        // Center vertex
        m.position.push_back({0.0f, 0.0f, terrain.evaluate_height(0.0f, 0.0f) + offset});
        m.uv.push_back({0.5f, 0.5f});

        for (int ri = 1; ri < (int)radii.size(); ++ri)
        {
            float r = radii[ri];
            for (int i = 0; i < N; ++i)
            {
                float theta = 2.0f * Pi * i / N;
                float x = r * std::cos(theta);
                float y = r * std::sin(theta);
                float z = terrain.evaluate_height(x, y) + offset;
                m.position.push_back({x, y, z});
                float u = 0.5f + 0.5f * (r / radii.back()) * std::cos(theta);
                float v = 0.5f + 0.5f * (r / radii.back()) * std::sin(theta);
                m.uv.push_back({u, v});
            }
        }

        // Center fan (ring 0 → ring 1)
        for (int i = 0; i < N; ++i)
        {
            int j = (i + 1) % N;
            m.connectivity.push_back({0,
                                      (uint32_t)(1 + i),
                                      (uint32_t)(1 + j)});
        }
        // Quads between consecutive rings
        for (int ri = 0; ri < (int)radii.size() - 2; ++ri)
        {
            uint32_t base_inner = 1 + ri * N;
            uint32_t base_outer = 1 + (ri + 1) * N;
            for (int i = 0; i < N; ++i)
            {
                int j = (i + 1) % N;
                m.connectivity.push_back({base_inner + i, base_outer + i, base_outer + j});
                m.connectivity.push_back({base_inner + i, base_outer + j, base_inner + j});
            }
        }

        m.normal_update();
        m.fill_empty_field();
        lava_pool.initialize_data_on_gpu(m);
        lava_pool.shader = shader_lava;
        lava_pool.material.phong.ambient = 1.0f;
        lava_pool.material.phong.diffuse = 0.0f;
        lava_pool.material.phong.specular = 0.0f;
    }

    // --- Skybox ---
    {
        image_structure image_skybox_template = image_load_file(project::path + "assets/cubemap.png");
        // image_split_grid(img, 4, 3): index = kv + 3*kh (column-major, kh=col, kv=row)
        //   [0]=empty  [3]=sky   [6]=empty  [9]=empty
        //   [1]=left   [4]=front [7]=right  [10]=back
        //   [2]=empty  [5]=bot   [8]=empty  [11]=empty
        std::vector<image_structure> image_grid = image_split_grid(image_skybox_template, 4, 3);
        skybox.initialize_data_on_gpu();
        // Same mapping as the TD reference (image was authored for Y-up convention)
        skybox.texture.initialize_cubemap_on_gpu(
            image_grid[1],  // x_neg : left
            image_grid[7],  // x_pos : right
            image_grid[5],  // y_neg : bottom/ground
            image_grid[3],  // y_pos : sky/top
            image_grid[10], // z_neg : back
            image_grid[4]   // z_pos : front (sun)
        );
        // The image was designed for Y-up convention, but the scene uses Z-up.
        // Apply a -90° rotation around X to map Z-up sampling directions to Y-up:
        //   world (0,0,+1) [sky]     → samples (0,+1, 0) → GL_POSITIVE_Y = sky  ✓
        //   world (0,0,-1) [ground]  → samples (0,-1, 0) → GL_NEGATIVE_Y = bot  ✓
        //   world (±1,0,0) [sides]   → unchanged                                  ✓
        skybox.skybox_rotation = mat3(
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, -1.0f, 0.0f);
    }

    // ---- Lava particles ----
    lava_system.initialize(crater_pos, shader_fog);
    lava_system.emission_rate = emission_rate;
    lava_system.velocity_scale = velocity_scale;

    // ---- Smoke ----
    smoke_system.initialize(crater_pos + vec3{0, 0, 0.3f});
    smoke_system.emission_rate = smoke_rate;

    // ---- Trees ----
    trees.initialize(shader_fog, [this](float x, float y)
                     { return terrain.evaluate_height(x, y); });

    // ---- Grass ----
    grass.initialize(shader_grass, [this](float x, float y)
                     { return terrain.evaluate_height(x, y); });

    std::cout << "[Volcano] Scene ready.\n";
}

// ============================================================
// DISPLAY FRAME
// ============================================================
void scene_structure::display_frame()
{
    camera_projection.aspect_ratio = window.aspect_ratio();
    environment.camera_projection = camera_projection.matrix();
    environment.camera_view = camera_control.camera_model.matrix_view();
    environment.light = camera_control.camera_model.position();

    if (gui.display_frame)
        draw(global_frame, environment);

    // Skybox — drawn first, no depth writes so it stays behind everything
    glDepthMask(GL_FALSE);
    draw(skybox, environment);
    glDepthMask(GL_TRUE);

    // Advance animation time (timer.update() avoids large jumps on unpause)
    float real_dt = timer.update();
    if (!gui.pause_animation)
    {
        anim_time += real_dt;
        anim_dt = real_dt;
    }
    else
    {
        anim_dt = 0.0f;
    }

    // Push fog + light + time uniforms for all shaders that need them
    environment.uniform_generic.uniform_float["fog_distance"] = fog_distance;
    environment.uniform_generic.uniform_vec3["fog_color"] = fog_color;
    environment.uniform_generic.uniform_float["time"] = anim_time;
    environment.uniform_generic.uniform_vec3["light_color"] = light_color;
    environment.uniform_generic.uniform_float["light_range"] = light_range;

    // Sync GUI parameters to systems
    lava_system.emission_rate = emission_rate;
    lava_system.velocity_scale = velocity_scale;
    smoke_system.emission_rate = smoke_rate;

    // ---- 1) Update simulations ----
    lava_system.update(anim_time, anim_dt);
    smoke_system.update(anim_time, anim_dt);

    // ---- 2) Opaque objects ----
    terrain.draw(environment, gui.display_wireframe);
    trees.draw(environment, gui.display_wireframe);

    // ---- 3) Lava pool (emissive shader — suppress missing-uniform warnings) ----
    draw(lava_pool, environment, 1, false);

    // ---- 4) Lava particles (emissive spheres) ----
    lava_system.draw(environment);

    // ---- 5) Grass (discard-based transparency, no blending needed) ----
    grass.draw(environment);

    // ---- 6) Smoke (semi-transparent billboards — must be last + sorted) ----
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    vec3 cam_pos = camera_control.camera_model.position();
    smoke_system.draw(environment, cam_pos, environment.camera_view);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// ============================================================
// GUI
// ============================================================
void scene_structure::display_gui()
{
    ImGui::Text("== Volcano Controls ==");
    ImGui::Separator();

    ImGui::Checkbox("Pause animation", &gui.pause_animation);
    ImGui::Checkbox("Show frame", &gui.display_frame);
    ImGui::Checkbox("Wireframe", &gui.display_wireframe);

    ImGui::Separator();
    ImGui::Text("Eruption");
    ImGui::SliderFloat("Emission rate (part/s)", &emission_rate, 0.0f, 200.0f);
    ImGui::SliderFloat("Lava velocity scale", &velocity_scale, 0.2f, 3.0f);

    if (ImGui::Button("BOOM ! Mega eruption"))
    {
        lava_system.mega_eruption(timer.t);
    }

    ImGui::Separator();
    ImGui::Text("Smoke");
    ImGui::SliderFloat("Smoke rate (part/s)", &smoke_rate, 0.0f, 20.0f);

    ImGui::Separator();
    ImGui::Text("Terrain (Perlin)");
    bool terrain_changed = false;
    terrain_changed |= ImGui::SliderFloat("Frequency", &terrain.perlin_frequency, 0.01f, 0.5f);
    terrain_changed |= ImGui::SliderInt("Octaves", &terrain.perlin_octaves, 1, 10);
    terrain_changed |= ImGui::SliderFloat("Persistence", &terrain.perlin_persistence, 0.1f, 0.9f);
    if (terrain_changed)
        terrain.rebuild();

    ImGui::Separator();
    ImGui::Text("Light");
    ImGui::ColorEdit3("Light color", &light_color.x);
    ImGui::SliderFloat("Light range", &light_range, 5.0f, 150.0f);

    ImGui::Separator();
    ImGui::Text("Atmosphere");
    ImGui::SliderFloat("Fog distance", &fog_distance, 5.0f, 80.0f);
    ImGui::ColorEdit3("Fog color", &fog_color.x);
    // Keep background in sync with fog
    environment.background_color = fog_color;

    ImGui::Separator();
    ImGui::Text("Camera");
    float fov_deg = camera_projection.field_of_view * 180.0f / Pi;
    if (ImGui::SliderFloat("Field of view (deg)", &fov_deg, 10.0f, 120.0f))
        camera_projection.field_of_view = fov_deg * Pi / 180.0f;
}

// ============================================================
// CALLBACKS
// ============================================================
void scene_structure::mouse_move_event()
{
    if (!inputs.keyboard.shift)
        camera_control.action_mouse_move();
}
void scene_structure::mouse_click_event() { camera_control.action_mouse_click(); }
void scene_structure::keyboard_event() { camera_control.action_keyboard(); }
void scene_structure::idle_frame() { camera_control.idle_frame(); }

void scene_structure::display_info()
{
    std::cout << "\nCAMERA CONTROL:\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << camera_control.doc_usage() << "\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << "\nSCENE: Volcano eruption\n";
    std::cout << "  Drag mouse to rotate, scroll to zoom\n";
    std::cout << "  GUI sliders: tweak emission, smoke, fog\n";
    std::cout << "  BOOM button: mega eruption!\n\n";
}
