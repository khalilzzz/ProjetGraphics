#pragma once

#include "cgp/cgp.hpp"
#include "environment.hpp"
#include "terrain.hpp"
#include "lava_particles.hpp"
#include "smoke.hpp"
#include "tree.hpp"
#include "grass.hpp"

using cgp::mesh_drawable;

struct gui_parameters {
    bool display_frame     = false;
    bool display_wireframe = false;
    bool pause_animation   = false;
};

struct scene_structure : cgp::scene_inputs_generic {

    // *** Standard CGP boilerplate ***
    void initialize();
    void display_frame();
    void display_gui();

    environment_structure environment;
    window_structure      window;
    input_devices         inputs;
    gui_parameters        gui;

    void display_info();

    cgp::camera_controller_orbit_euler camera_control;
    cgp::camera_projection_perspective  camera_projection;

    // *** Scene elements ***
    cgp::mesh_drawable global_frame;
    cgp::timer_basic   timer;

    terrain_structure        terrain;
    lava_particles_structure lava_system;
    smoke_structure          smoke_system;
    tree_structure           trees;
    grass_structure          grass;

    cgp::mesh_drawable lava_pool;   // animated lava surface at crater

    // *** Shaders ***
    cgp::opengl_shader_structure shader_fog;
    cgp::opengl_shader_structure shader_lava;
    cgp::opengl_shader_structure shader_grass;

    // *** Animation time (controlled manually so pause works) ***
    float anim_time = 0.0f;
    float anim_dt   = 0.0f;

    // *** GUI-controlled parameters ***
    float emission_rate   = 60.0f;
    float velocity_scale  = 1.0f;
    float smoke_rate      = 4.0f;
    float fog_distance    = 30.0f;
    cgp::vec3 fog_color   = {0.55f, 0.45f, 0.40f};

    // Light
    cgp::vec3 light_color = {1.0f, 1.0f, 1.0f};
    float     light_range = 60.0f;

    // *** Callbacks ***
    void mouse_move_event();
    void mouse_click_event();
    void keyboard_event();
    void idle_frame();
};
