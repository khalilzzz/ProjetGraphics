#include "scene.hpp"
#include <cmath>

using namespace cgp;

void scene_structure::initialize()
{
    std::cout << "[Volcano] Initializing scene...\n";

    /* mise en place de la caméra orbitale et du frustum perspective de la scène */
    camera_control.initialize(inputs, window);
    camera_control.set_rotation_axis_z();
    camera_control.look_at({20.0f, -12.0f, 12.0f}, {0, 0, 3}, {0, 0, 1});

    camera_projection = camera_projection_perspective{
        50.0f * Pi / 180.0f, 1.0f, 0.05f, 500.0f};

    /* on aligne la couleur de fond sur la couleur du brouillard pour eviter une demarcation a l'horizon */
    environment.background_color = fog_color;

    global_frame.initialize_data_on_gpu(mesh_primitive_frame());

    /* chargement des trois shaders custom utilises dans la scene */
    shader_fog.load(project::path + "shaders/mesh_fog/mesh_fog.vert.glsl",
                    project::path + "shaders/mesh_fog/mesh_fog.frag.glsl");

    shader_lava.load(project::path + "shaders/lava/lava.vert.glsl",
                     project::path + "shaders/lava/lava.frag.glsl");

    shader_grass.load(project::path + "shaders/grass/grass.vert.glsl",
                      project::path + "shaders/grass/grass.frag.glsl");

    terrain.initialize(shader_fog);

    /* on releve la hauteur exacte du sommet du cratere pour positionner l'emetteur de lave et de fumee */
    float crater_z = terrain.evaluate_height(0.0f, 0.0f);
    vec3 crater_pos = {0.0f, 0.0f, crater_z};
    std::cout << "[Volcano] Crater z = " << crater_z << "\n";

    /* nappe de lave du cratere : on construit un maillage en anneaux concentriques dont chaque sommet
       echantillonne evaluate_height, pour que la surface epouse exactement le terrain au lieu de flotter */
    {
        int N = 60;
        std::vector<float> radii = {0.0f, 0.55f, 1.1f, 1.65f, 2.2f};
        float offset = 0.12f; /* leger decalage vertical pour eviter le z-fighting avec le terrain */
        mesh m;

        /* sommet central de l'eventail, puis on parcourt chaque anneau pour generer les sommets */
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

        /* triangles de l'eventail central reliant le sommet (0) au premier anneau */
        for (int i = 0; i < N; ++i)
        {
            int j = (i + 1) % N;
            m.connectivity.push_back({0,
                                      (uint32_t)(1 + i),
                                      (uint32_t)(1 + j)});
        }
        /* quads (decoupes en deux triangles) entre chaque paire d'anneaux successifs */
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

    /* skybox : on decoupe l'image cubemap.png en 12 tuiles dans une grille 4x3, puis on assigne
       chaque tuile a une face du cubemap, et on applique une rotation pour passer de la convention
       Y-up de l'image source a la convention Z-up de la scene. (mapping et rotation : aide IA) */
    {
        image_structure image_skybox_template = image_load_file(project::path + "assets/cubemap.png");
        std::vector<image_structure> image_grid = image_split_grid(image_skybox_template, 4, 3);
        skybox.initialize_data_on_gpu();
        skybox.texture.initialize_cubemap_on_gpu(
            image_grid[1],  /* x_neg : gauche */
            image_grid[7],  /* x_pos : droite */
            image_grid[5],  /* y_neg : sol */
            image_grid[3],  /* y_pos : ciel */
            image_grid[10], /* z_neg : arriere */
            image_grid[4]   /* z_pos : avant (soleil) */
        );
        skybox.skybox_rotation = mat3(
            1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, -1.0f, 0.0f);
    }

    /* initialisation des systemes de particules de lave et de fumee a la position du cratere */
    lava_system.initialize(crater_pos, shader_fog);
    lava_system.emission_rate = emission_rate;
    lava_system.velocity_scale = velocity_scale;

    smoke_system.initialize(crater_pos + vec3{0, 0, 0.3f});
    smoke_system.emission_rate = smoke_rate;

    /* on passe a chaque systeme une fonction qui evalue la hauteur du terrain
       pour qu'arbres et herbe soient places exactement sur le sol */
    trees.initialize(shader_fog, [this](float x, float y)
                     { return terrain.evaluate_height(x, y); });

    grass.initialize(shader_grass, [this](float x, float y)
                     { return terrain.evaluate_height(x, y); });

    std::cout << "[Volcano] Scene ready.\n";
}

void scene_structure::display_frame()
{
    camera_projection.aspect_ratio = window.aspect_ratio();
    environment.camera_projection = camera_projection.matrix();
    environment.camera_view = camera_control.camera_model.matrix_view();
    environment.light = camera_control.camera_model.position();

    if (gui.display_frame)
        draw(global_frame, environment);

    /* la skybox est dessinee en premier avec depth mask off : ainsi sa profondeur
       n'est jamais ecrite et tous les autres objets passent devant naturellement */
    glDepthMask(GL_FALSE);
    draw(skybox, environment);
    glDepthMask(GL_TRUE);

    /* on avance manuellement le temps d'animation pour que la pause stoppe les particules
       (timer.update() est appele dans tous les cas pour garder dt coherent quand on reprend) */
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

    /* on pousse tous les uniforms partages (fog, lumiere, temps) une seule fois par frame */
    environment.uniform_generic.uniform_float["fog_distance"] = fog_distance;
    environment.uniform_generic.uniform_vec3["fog_color"] = fog_color;
    environment.uniform_generic.uniform_float["time"] = anim_time;
    environment.uniform_generic.uniform_vec3["light_color"] = light_color;
    environment.uniform_generic.uniform_float["light_range"] = light_range;
    /* la position de la camera est constante pour toute la frame : on la calcule une fois cote C++
       et on la pousse en uniform plutot que de la reconstruire dans chaque fragment a partir de la vue */
    vec3 cam_pos = camera_control.camera_model.position();
    environment.uniform_generic.uniform_vec3["camera_position"] = cam_pos;

    /* propagation des sliders GUI vers les systemes de particules */
    lava_system.emission_rate = emission_rate;
    lava_system.velocity_scale = velocity_scale;
    smoke_system.emission_rate = smoke_rate;

    /* mise a jour des simulations (particules de lave en chute libre, particules de fumee en spirale) */
    lava_system.update(anim_time, anim_dt);
    smoke_system.update(anim_time, anim_dt);

    /* objets opaques : terrain et arbres */
    terrain.draw(environment, gui.display_wireframe);
    trees.draw(environment, gui.display_wireframe);

    /* nappe de lave emissive (shader lava sans illumination Phong complete) */
    draw(lava_pool, environment, 1, false);

    /* particules de lave : spheres orange traitees comme objets opaques */
    lava_system.draw(environment);

    /* herbe : alpha discard dans le shader, donc opaque du point de vue OpenGL, pas de tri */
    grass.draw(environment);

    /* fumee semi-transparente : on active l'alpha blending, on coupe le depth write
       (le test reste actif), on dessine en dernier dans l'ordre back-to-front (tri fait
       dans smoke_system.draw), puis on restaure l'etat OpenGL. (bloc genere par IA) */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    smoke_system.draw(environment, cam_pos, environment.camera_view);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void scene_structure::display_gui()
{
    ImGui::Text("boutons de controle");
    ImGui::Separator();

    ImGui::Checkbox("Pause animation", &gui.pause_animation);
    ImGui::Checkbox("Show frame", &gui.display_frame);
    ImGui::Checkbox("Wireframe", &gui.display_wireframe);

    ImGui::Separator();
    ImGui::Text("Eruption");
    ImGui::SliderFloat("Emission rate (part/s)", &emission_rate, 0.0f, 200.0f);
    ImGui::SliderFloat("Lava velocity scale", &velocity_scale, 0.2f, 3.0f);

    if (ImGui::Button("Mega eruption"))
    {
        lava_system.mega_eruption(timer.t);
    }

    ImGui::Separator();
    ImGui::Text("Smoke");
    ImGui::SliderFloat("Smoke rate (part/s)", &smoke_rate, 0.0f, 20.0f);

    ImGui::Separator();
    ImGui::Text("Terrain (Perlin)");
    bool terrain_changed = false;
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
    /* on garde le fond de fenetre synchronise avec la couleur du brouillard a chaque frame */
    environment.background_color = fog_color;

    ImGui::Separator();
    ImGui::Text("Camera");
    float fov_deg = camera_projection.field_of_view * 180.0f / Pi;
    if (ImGui::SliderFloat("Field of view (deg)", &fov_deg, 10.0f, 120.0f))
        camera_projection.field_of_view = fov_deg * Pi / 180.0f;
}

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
