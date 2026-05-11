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
        50.0f * Pi / 180.0f, 1.0f, 0.05f, 500.0f
    };

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
    {
        int N = 50;
        float r_pool = 2.2f;
        mesh m;
        // Center vertex
        m.position.push_back({0.0f, 0.0f, crater_z + 0.05f});
        m.uv.push_back({0.5f, 0.5f});
        for (int i = 0; i < N; ++i) {
            float theta = 2.0f * Pi * i / N;
            float x = r_pool * std::cos(theta);
            float y = r_pool * std::sin(theta);
            float z = terrain.evaluate_height(x, y) + 0.05f;
            m.position.push_back({x, y, z});
            m.uv.push_back({0.5f + 0.5f * std::cos(theta),
                            0.5f + 0.5f * std::sin(theta)});
        }
        for (int i = 0; i < N; ++i) {
            int j = (i + 1) % N;
            m.connectivity.push_back({0, (uint32_t)(i+1), (uint32_t)(j+1)});
        }
        m.normal_update();
        m.fill_empty_field();
        lava_pool.initialize_data_on_gpu(m);
        lava_pool.shader = shader_lava;
        lava_pool.material.phong.ambient  = 1.0f;
        lava_pool.material.phong.diffuse  = 0.0f;
        lava_pool.material.phong.specular = 0.0f;
    }

    // ---- Skybox ----
    {
        image_structure image_skybox_template = image_load_file(project::path + "assets/cubemap.png");
        std::vector<image_structure> image_grid = image_split_grid(image_skybox_template, 4, 3);
        // Column-major indices (kv + 3*kh):
        //   Row 0: [0]    [3]=sky  [6]     [9]
        //   Row 1: [1]=L  [4]=fire [7]=R   [10]=back
        //   Row 2: [2]    [5]=bot  [8]     [11]
        // CGP is Z-up: +Z=sky → z_pos, -Z=ground → z_neg
        skybox.initialize_data_on_gpu();
        skybox.texture.initialize_cubemap_on_gpu(
            image_grid[1],   // x_neg : left
            image_grid[7],   // x_pos : right
            image_grid[10],  // y_neg : back
            image_grid[4],   // y_pos : front (fire/sun)
            image_grid[5],   // z_neg : bottom/ground
            image_grid[3]    // z_pos : sky/top
        );
    }

    // ---- Lava particles ----
    lava_system.initialize(crater_pos, shader_fog);
    lava_system.emission_rate = emission_rate;
    lava_system.velocity_scale = velocity_scale;

    // ---- Smoke ----
    smoke_system.initialize(crater_pos + vec3{0, 0, 0.3f});
    smoke_system.emission_rate = smoke_rate;

    // ---- Trees ----
    trees.initialize(shader_fog, [this](float x, float y){
        return terrain.evaluate_height(x, y);
    });

    // ---- Grass ----
    grass.initialize(shader_grass, [this](float x, float y){
        return terrain.evaluate_height(x, y);
    });

    std::cout << "[Volcano] Scene ready.\n";
}

// ============================================================
// DISPLAY FRAME
// ============================================================
void scene_structure::display_frame()
{
    camera_projection.aspect_ratio = window.aspect_ratio();
    environment.camera_projection   = camera_projection.matrix();
    environment.camera_view         = camera_control.camera_model.matrix_view();
    environment.light               = camera_control.camera_model.position();

    if (gui.display_frame)
        draw(global_frame, environment);

    // Skybox — drawn first, no depth writes so it stays behind everything
    glDepthMask(GL_FALSE);
    draw(skybox, environment);
    glDepthMask(GL_TRUE);

    // Advance animation time (timer.update() avoids large jumps on unpause)
    float real_dt = timer.update();
    if (!gui.pause_animation) {
        anim_time += real_dt;
        anim_dt    = real_dt;
    } else {
        anim_dt = 0.0f;
    }

    // Push fog + light + time uniforms for all shaders that need them
    environment.uniform_generic.uniform_float["fog_distance"] = fog_distance;
    environment.uniform_generic.uniform_vec3 ["fog_color"]    = fog_color;
    environment.uniform_generic.uniform_float["time"]         = anim_time;
    environment.uniform_generic.uniform_vec3 ["light_color"]  = light_color;
    environment.uniform_generic.uniform_float["light_range"]  = light_range;

    // Sync GUI parameters to systems
    lava_system.emission_rate  = emission_rate;
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
    ImGui::Checkbox("Show frame",      &gui.display_frame);
    ImGui::Checkbox("Wireframe",       &gui.display_wireframe);

    ImGui::Separator();
    ImGui::Text("Eruption");
    ImGui::SliderFloat("Emission rate (part/s)", &emission_rate,  0.0f, 200.0f);
    ImGui::SliderFloat("Lava velocity scale",    &velocity_scale, 0.2f,   3.0f);

    if (ImGui::Button("BOOM ! Mega eruption")) {
        lava_system.mega_eruption(timer.t);
    }

    ImGui::Separator();
    ImGui::Text("Smoke");
    ImGui::SliderFloat("Smoke rate (part/s)", &smoke_rate, 0.0f, 20.0f);

    ImGui::Separator();
    ImGui::Text("Terrain (Perlin)");
    bool terrain_changed = false;
    terrain_changed |= ImGui::SliderFloat("Frequency",   &terrain.perlin_frequency,   0.01f, 0.5f);
    terrain_changed |= ImGui::SliderInt  ("Octaves",     &terrain.perlin_octaves,     1, 10);
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
    ImGui::ColorEdit3("Fog color",     &fog_color.x);
    // Keep background in sync with fog
    environment.background_color = fog_color;
}

// ============================================================
// CALLBACKS
// ============================================================
void scene_structure::mouse_move_event()
{
    if (!inputs.keyboard.shift)
        camera_control.action_mouse_move();
}
void scene_structure::mouse_click_event()  { camera_control.action_mouse_click(); }
void scene_structure::keyboard_event()     { camera_control.action_keyboard(); }
void scene_structure::idle_frame()         { camera_control.idle_frame(); }

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
