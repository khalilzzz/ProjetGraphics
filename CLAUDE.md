# Plan complet — Scène 3D « Volcan en éruption »

Projet CSC_43043_EP, basé sur la bibliothèque CGP et le template `ProjetGraphics`.

---

## Vue d'ensemble

La scène se compose de cinq grands éléments visuels :

1. **Le terrain volcanique** : champ de hauteur procédural (Gaussienne + bruit de Perlin) avec texture, formant un cône avec un cratère.
2. **L'éruption** : particules de lave éjectées du cratère suivant une trajectoire de chute libre (parabole).
3. **Les coulées de lave** : surface texturée et animée descendant les flancs du volcan.
4. **Fumée et fumerolles** : billboards semi-transparents au-dessus du cratère.
5. **Forêt et herbe** : arbres modélisés par primitives + billboards d'herbe répartis autour du volcan.

L'ensemble est plongé dans un **brouillard atmosphérique** (effet de fog dans le fragment shader) qui renforce l'ambiance et masque le plan de clipping. C'est exactement le rendu de ton image `Volcano.png`.

**Techniques de cours utilisées** : surfaces paramétriques, bruit de Perlin (terrain et lave), particules en chute libre, billboards, transparence avec tri par profondeur (ou `discard`), textures avec coordonnées UV, shaders custom (fog, lave émissive), illumination de Phong.

---

## Phase 0 — Organisation des fichiers

Avant tout, structure ton dossier `src/` autour de classes dédiées. Tu copies/adaptes le template `ProjetGraphics` :

```
src/
├── main.cpp
├── application.cpp / .hpp        (déjà fournis)
├── environment.cpp / .hpp        (déjà fournis)
├── scene.cpp / .hpp              (à étendre, c'est ton chef d'orchestre)
├── terrain.cpp / .hpp            (terrain + cratère)
├── tree.cpp / .hpp               (modélisation des arbres)
├── lava_particles.cpp / .hpp     (système de particules de lave)
├── smoke.cpp / .hpp              (système de particules de fumée + billboards)
└── grass.cpp / .hpp              (billboards d'herbe)

shaders/
├── mesh/                         (shader par défaut, déjà fourni)
├── mesh_fog/                     (mesh.frag.glsl modifié pour le brouillard)
├── lava/                         (lava.frag.glsl avec animation Perlin et émission)
└── billboard/                    (pour fumée + herbe, gère le discard)

assets/
├── lava.png
├── smoke.png        (avec canal alpha)
├── grass.png        (avec canal alpha)
├── tree_bark.jpg
├── tree_leaves.png  (avec canal alpha, optionnel)
└── ground_rock.jpg
```

Chaque nouvelle structure (`lava_particles_structure`, `smoke_structure`, etc.) suit le même schéma que `terrain_structure` du TP : un `.hpp` avec les variables membres et les signatures, un `.cpp` avec l'implémentation, et une fonction `initialize()` appelée depuis `scene.cpp`, plus une fonction `update(dt)` et `draw(environment)`.

---

## Phase 1 — Le terrain volcanique

### 1.1 Forme générale du volcan

Le terrain est défini par `z = f(x, y)`, comme dans le TP terrain. Mais cette fois, la forme du volcan est obtenue en **superposant** trois ingrédients :

```
z(x, y) = h_volcan(r) + h_perlin(x, y) + h_cratere(r)
```

où `r = sqrt(x² + y²)` est la distance au centre.

**(a) Profil conique du volcan** — une Gaussienne centrée :
```cpp
float h_volcan = h_max * exp(-r*r / (2*sigma*sigma));
```
Choisis par exemple `h_max = 6.0`, `sigma = 5.0` sur un terrain de taille `length = 40`.

**(b) Bruit de Perlin pour le relief naturel** — ajoute des aspérités avec plusieurs octaves :
```cpp
float h_perlin = amplitude * noise_perlin({x*frequency, y*frequency}, octaves, persistence);
```
Paramètres typiques : `octaves = 5`, `persistence = 0.45`, `frequency = 0.15`, `amplitude = 1.2`. Tu retrouves ça dans le TP `07_texture/c_perlin_noise/`.

**(c) Le cratère** — Gaussienne **négative** à la pointe :
```cpp
float h_cratere = -profondeur * exp(-r*r / (2*sigma_cratere*sigma_cratere));
```
Avec `profondeur = 1.5` et `sigma_cratere = 1.2`, tu creuses un cratère sur le sommet.

### 1.2 Création du maillage

Dans `terrain.cpp`, complète `create_mesh()` comme dans le TP modeling. Augmente la résolution à `N = 200` ou `300` pour que le bruit de Perlin soit bien visible.

**Important** : recalcule les normales après avoir appliqué `evaluate_height`, sinon ton illumination de Phong sera fausse. La fonction `mesh.normal_update()` de CGP s'en occupe.

### 1.3 Texture du terrain

Charge une texture de roche/sol volcanique. Coordonnées UV en répétition :
```cpp
terrain.uv[kv + N*ku] = {ku * 4.0f / (N-1), kv * 4.0f / (N-1)}; // répétition x4
```
Active le mode `GL_REPEAT` sur la texture.

Tu peux aussi colorier procéduralement les sommets selon la hauteur (gris-noir pour la pierre, rouge sombre près du cratère) en plus de la texture — ça donne plus de variété.

---

## Phase 2 — Système de particules de lave (cœur de l'animation)

### 2.1 Structure d'une particule

Dans `lava_particles.hpp` :

```cpp
struct lava_particle {
    cgp::vec3 p0;       // position d'émission
    cgp::vec3 v0;       // vitesse initiale
    float t0;           // temps d'émission
    float lifetime;     // durée de vie
    float size;         // rayon visuel
};

struct lava_particles_structure {
    std::vector<lava_particle> particles;
    cgp::mesh_drawable sphere_drawable;       // sphère réutilisée (instancing visuel)
    cgp::vec3 emitter_position;               // sommet du cratère
    float emission_rate = 80.0f;              // particules / seconde
    float time_accumulator = 0.0f;

    void initialize();
    void emit_particle(float current_time);
    void update(float current_time, float dt);
    void draw(environment_structure const& env);
};
```

### 2.2 Émission depuis le cratère

À chaque frame, en fonction de `emission_rate`, tu crées de nouvelles particules. La position de départ est le centre du cratère (un peu en dessous du niveau du sol pour qu'elles "jaillissent") avec un petit décalage aléatoire dans un disque, et la vitesse initiale est principalement verticale :

```cpp
vec3 p_init = emitter_position + vec3(rand_uniform(-0.2f, 0.2f),
                                       rand_uniform(-0.2f, 0.2f),
                                       0);
vec3 v_init = vec3(rand_uniform(-1.5f, 1.5f),       // dispersion x
                   rand_uniform(-1.5f, 1.5f),       // dispersion y
                   rand_uniform(7.0f, 10.0f));      // poussée vers le haut
```

### 2.3 Trajectoire de chute libre

C'est exactement la formule du cours (Newton, double intégration) :
```
p(t) = ½ g t² + v₀ t + p₀
```
avec `g = (0, 0, -9.81)` (en convention "z vers le haut" comme CGP).

Dans `update()`, pour chaque particule vivante :
```cpp
float dt_p = current_time - p.t0;
vec3 pos = 0.5f * g * dt_p * dt_p + p.v0 * dt_p + p.p0;
```

Tu obtiens la **parabole** caractéristique : chaque goutte de lave monte, ralentit, retombe.

### 2.4 Mort et recyclage

Quand `dt_p > p.lifetime` ou quand `pos.z` est passé sous le niveau du terrain, marque la particule comme morte et réutilise son emplacement (cf. cours section 4.1, recyclage des particules).

### 2.5 Affichage

Solution simple pour commencer : afficher chaque particule comme une petite sphère orange émissive :
```cpp
sphere_drawable.model.translation = pos;
sphere_drawable.model.scaling = p.size;
sphere_drawable.material.color = {1.0f, 0.4f, 0.05f}; // orange-rouge
sphere_drawable.material.phong.ambient = 0.9f;       // émissive : peu sensible à la lumière
draw(sphere_drawable, environment);
```

**Évolution possible** (à faire après que tout marche) : passer aux **billboards** texturés avec une texture de feu pour gagner en réalisme et en performance — c'est ce qui est décrit en section 4.2 du cours.

---

## Phase 3 — Coulées de lave sur les flancs

Pour donner l'impression que la lave coule (comme sur ton image), tu peux modéliser une **surface en anneau** autour du cratère qui descend les flancs, et l'animer avec du Perlin :

### 3.1 Géométrie

Soit tu reprends le même maillage que le terrain mais en n'affichant que les triangles dans une couronne `r_min < r < r_max`, soit tu construis un anneau dédié.

Plus simple : utilise un **disque déformé** placé légèrement au-dessus du terrain dans la zone du sommet.

### 3.2 Shader de lave animée

Crée `shaders/lava/lava.frag.glsl` qui :

- N'applique **pas** l'illumination de Phong (la lave est émissive).
- Utilise une texture de lave, mais déforme les coordonnées UV avec du **bruit de Perlin animé** (la 3ème dimension étant le temps) — c'est le cas d'usage évoqué section 4.5 « Évolution lisse : `P(x, y, t)` qui donne un effet de lave qui bouillonne ».

```glsl
uniform float time;
uniform sampler2D image_texture;

void main() {
    vec2 uv_offset = vec2(noise3D(fragment.position.x, fragment.position.y, 0.3*time),
                          noise3D(fragment.position.x+5.0, fragment.position.y+5.0, 0.3*time));
    vec3 lava_color = texture(image_texture, fragment.uv + 0.05*uv_offset).rgb;

    // émissive : couleur directe sans Phong
    FragColor = vec4(lava_color, 1.0);
}
```

Tu enverras `time = timer.t` depuis `display_frame()` via `environment.uniform_generic.uniform_float["time"]`, comme dans le TP shader.

Tu peux implémenter un Perlin GLSL simple ou passer le bruit comme **texture 2D précalculée** que tu fais défiler (`uv + vec2(0, time*0.1)`) — beaucoup plus simple et tout aussi convaincant.

---

## Phase 4 — Fumée au-dessus du cratère

C'est un second système de particules, mais avec billboards.

### 4.1 Géométrie : billboard sphérique

Chaque particule de fumée est un quadrangle qui **fait face à la caméra** (cf. cours section 4.2). À chaque frame, tu construis sa matrice d'orientation à partir des colonnes de la `view` :

```cpp
// extraire right et up de la matrice de vue
mat3 view_rot = transpose(mat3(environment.camera_view));
vec3 right = view_rot * vec3(1,0,0);
vec3 up    = view_rot * vec3(0,1,0);
```

Pour ta scène (volcan vu de l'extérieur), tu peux aussi te contenter d'un **billboard cylindrique** (orienté autour de z), c'est plus stable visuellement.

### 4.2 Trajectoire procédurale en spirale ascendante

Plutôt que de la physique pure, utilise la trajectoire en spirale du cours :
```cpp
float t = current_time - p.t0;
pos.x = p.p0.x + 0.5f * t * cos(2.0f * t);
pos.y = p.p0.y + 0.5f * t * sin(2.0f * t);
pos.z = p.p0.z + 1.5f * t;     // monte régulièrement
```

La fumée monte en tournant doucement, ce qui donne le tourbillon ascendant typique des éruptions.

Évolution des attributs visuels :
- **Taille** qui grandit avec le temps : `size = size_init * (1 + 0.5*t)`
- **Transparence** qui augmente vers la fin de vie : `alpha = 1.0 - t/lifetime`

### 4.3 Texture et rendu transparent

Charge `smoke.png` (texture grise avec canal alpha). Pour le rendu, applique **strictement** la procédure du cours section 4.3 :

```cpp
// 1. Dessine d'abord tous les objets opaques (terrain, arbres, lave émissive, …)
draw_opaque();

// 2. Active le blending
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDepthMask(false);  // depth test reste actif, mais on n'écrit plus

// 3. Trie les particules de fumée par distance à la caméra (du plus loin au plus proche)
std::sort(smoke_particles.begin(), smoke_particles.end(),
    [&](auto const& a, auto const& b) {
        return norm(a.pos - camera_pos) > norm(b.pos - camera_pos);
    });

// 4. Dessine les billboards
for (auto const& p : smoke_particles) draw(p.billboard, environment);

// 5. Rétablis l'état
glDisable(GL_BLEND);
glDepthMask(true);
```

C'est *l'erreur classique* à ne pas commettre : oublier le tri ou laisser `depth mask` actif.

---

## Phase 5 — Les arbres

Tu reprends presque tel quel le code du TP modeling (section "Modélisation d'arbres sur le terrain").

### 5.1 Modélisation

Dans `tree.cpp`, conserve :
- `create_cylinder_mesh(radius, height)` pour le tronc
- `create_cone_mesh(radius, height, offset)` pour le feuillage (3 cônes empilés)
- `create_mesh()` qui assemble le tout

Mets de côté la couleur uniforme du TP : tu vas appliquer des **textures** :
- `tree_bark.jpg` sur le cylindre — assigne `tree_drawable_trunk.texture = ...`
- soit un vert procédural sur les cônes, soit `tree_leaves.png` (semi-transparent).

Pour ça, il faudra **séparer** les `mesh_drawable` du tronc et du feuillage (ne pas concaténer en un seul mesh comme dans le TP), parce qu'ils ont des textures différentes.

### 5.2 Placement intelligent

Reprends `terrain_structure::generate_positions(N)` du TP, mais avec deux contraintes supplémentaires pour ton volcan :

```cpp
std::vector<vec3> tree_positions;
while (tree_positions.size() < N_trees) {
    float x = rand_uniform(-length/2, length/2);
    float y = rand_uniform(-length/2, length/2);
    float r = sqrt(x*x + y*y);
    if (r < 8.0f) continue;        // pas trop près du volcan (zone brûlée)
    if (r > 18.0f) continue;        // pas trop loin (limite visuelle)
    float z = evaluate_height(x, y);
    tree_positions.push_back({x, y, z});
}
```

Puis dans `display_frame()`, boucle sur les positions et applique-les comme `model.translation` du même arbre — c'est de l'**instanciation** visuelle (cf. cours section 4.4 et figure 21 sur la matrice de modèle).

Pense à varier légèrement la **rotation** autour de z et le **scaling** de chaque arbre pour éviter qu'ils paraissent clonés :
```cpp
tree.model.rotation = rotation_transform::from_axis_angle({0,0,1}, rand_uniform(0, 2*Pi));
tree.model.scaling = rand_uniform(0.8f, 1.3f);
```

---

## Phase 6 — Herbe en billboards

L'herbe est l'exemple-type d'utilisation des billboards avec `discard`, déjà décrit dans le TP `07_texture/b_billboards/`.

### 6.1 Quadrangle texturé

Chaque touffe d'herbe est un quad avec `grass.png` (alpha pour la silhouette). Comme la texture est **soit opaque, soit totalement transparente** (pas de demi-tons), tu peux utiliser la technique simple du `discard` dans le fragment shader plutôt que le blending compliqué :

```glsl
vec4 c = texture(image_texture, fragment.uv);
if (c.a < 0.5) discard;
FragColor = vec4(c.rgb * color_shading, 1.0);
```

Avantages : pas besoin de désactiver le depth buffer, pas besoin de trier. C'est l'approche recommandée pour de la végétation.

### 6.2 Placement

Comme pour les arbres, mais avec une zone d'exclusion encore plus large autour du cratère (terre brûlée). Génère 1000–3000 touffes pour densifier le rendu.

Astuce : pour chaque position, place **2 ou 3 quadrangles croisés** (l'un à 0°, l'autre à 60°, l'autre à 120° autour de z). Comme ça l'herbe a du volume quel que soit l'angle de vue, sans avoir à toujours regarder la caméra.

---

## Phase 7 — Brouillard atmosphérique

C'est l'effet qui va donner l'ambiance « volcan dans la brume » de ton image. Tu suis l'énoncé du TP shader.

### 7.1 Modification du fragment shader

Copie `shaders/mesh/mesh.frag.glsl` dans `shaders/mesh_fog/mesh_fog.frag.glsl`, puis ajoute à la fin :

```glsl
uniform vec3 fog_color = vec3(0.7, 0.7, 0.75);  // gris légèrement bleuté
uniform float fog_distance = 25.0;

// position caméra (déjà extraite plus haut dans le shader)
float d = length(fragment.position - camera_position);
float alpha_fog = min(d / fog_distance, 1.0);

vec3 final_color = mix(color_shading, fog_color, alpha_fog);
FragColor = vec4(final_color, material.alpha * color_image_texture.a);
```

C'est exactement la formule du cours : `C = (1-α_f)·C_p + α_f·C_f`.

### 7.2 Cohérence du background

**Très important** : la couleur de fond de la fenêtre doit correspondre à la couleur du brouillard, sinon tu vois une démarcation nette à l'horizon. Dans `scene.cpp::initialize()` :
```cpp
environment.background_color = {0.7f, 0.7f, 0.75f};
```

### 7.3 Application

Affecte ce shader à tous les objets opaques (terrain, arbres) :
```cpp
opengl_shader_structure shader_fog;
shader_fog.load(project::path + "shaders/mesh_fog/mesh_fog.vert.glsl",
                project::path + "shaders/mesh_fog/mesh_fog.frag.glsl");
terrain.shader = shader_fog;
tree_trunk.shader = shader_fog;
tree_leaves.shader = shader_fog;
```

Pour la lave et la fumée, c'est différent : la lave est émissive (pas affectée par le brouillard, ou très peu), la fumée se mélange naturellement au brouillard via son alpha. Tu peux faire un shader spécial qui atténue moins la lave.

---

## Phase 8 — Interaction utilisateur (le « plus »)

Dans `display_gui()`, ajoute des sliders ImGui qui rendent ta scène vivante :

```cpp
ImGui::SliderFloat("Emission rate", &lava_system.emission_rate, 0, 200);
ImGui::SliderFloat("Lava velocity", &lava_velocity_magnitude, 5, 15);
ImGui::SliderFloat("Smoke density", &smoke_system.emission_rate, 0, 50);
ImGui::SliderFloat("Fog distance", &fog_distance, 5, 50);
ImGui::ColorEdit3("Fog color", &fog_color.x);
ImGui::Checkbox("Pause animation", &is_paused);
ImGui::Checkbox("Show wireframe", &gui.display_wireframe);

if (ImGui::Button("BOOM ! Méga éruption")) {
    // émet 500 particules d'un coup avec des vitesses fortes
    for (int k = 0; k < 500; ++k)
        lava_system.emit_particle(timer.t, big_eruption=true);
}
```

Pour le clavier/souris : la caméra `camera_controller_orbit_euler` du template suffit déjà. Tu peux ajouter une touche `R` pour relancer l'éruption depuis zéro, par exemple.

---

## Phase 9 — Ordre d'affichage dans `display_frame()`

L'**ordre est critique** quand il y a de la transparence. Voici l'ordre à respecter :

```cpp
void scene_structure::display_frame() {
    // ... mise à jour caméra, lumière, time uniform, timer.update() ...

    // 1) Objets OPAQUES
    draw(terrain, environment);
    for (auto const& pos : tree_positions) {
        tree_trunk.model.translation = pos;
        tree_leaves.model.translation = pos;
        draw(tree_trunk, environment);
        draw(tree_leaves, environment);
    }

    // 2) Lave (émissive, opaque, mais on l'affiche après le terrain pour l'overlay propre)
    lava_system.update(timer.t, timer.dt);
    lava_system.draw(environment);
    draw(lava_flow_surface, environment);  // les coulées texturées

    // 3) Herbe avec discard (toujours opaque du point de vue OpenGL, donc pas de souci)
    grass_system.draw(environment);

    // 4) Objets SEMI-TRANSPARENTS — fumée
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    smoke_system.update(timer.t, timer.dt);
    smoke_system.sort_by_camera_distance(camera_position);
    smoke_system.draw(environment);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
```

---

## Phase 10 — Étapes de développement recommandées

Procède par jalons testables. Compile et lance entre chaque étape, ne fais pas tout d'un coup.

| Jalon | Contenu | Vérification |
|-------|---------|--------------|
| **J1** | Terrain plat avec Perlin | Tu vois un relief naturel |
| **J2** | Ajoute la Gaussienne + cratère | Tu vois la forme du volcan |
| **J3** | Texture sur le terrain | Roche visible avec répétition correcte |
| **J4** | Une sphère qui monte/descend avec g = -9.81 | Trajectoire parabolique |
| **J5** | 100 particules de lave émises depuis le cratère | Jet visible |
| **J6** | Recyclage + couleurs orange émissives | Boucle stable, pas de fuite mémoire |
| **J7** | Un arbre placé manuellement | Modèle correct |
| **J8** | Multiples arbres avec exclusion autour du volcan | Forêt circulaire |
| **J9** | Une touffe d'herbe (quad + discard) | Silhouette d'herbe propre |
| **J10** | Champ d'herbe complet | Sol densifié |
| **J11** | Quelques billboards de fumée fixes | Texture transparente correcte (tri OK) |
| **J12** | Fumée animée en spirale | Mouvement vivant |
| **J13** | Shader de fog sur le terrain | Atténuation visible au loin |
| **J14** | Fog sur tous les objets opaques + background | Cohérence visuelle |
| **J15** | Coulées de lave avec UV animées | Effet "bouillonnant" |
| **J16** | GUI complète avec sliders | Tout paramétrable |
| **J17** | Polissage : lumière, couleurs, intensités | Ambiance volcanique convaincante |

---

## Idées d'extensions (bonus pour faire la différence)

Une fois la base solide, tu peux ajouter :

- **Rebonds des particules** au sol (cours section 4.1, équation `v_après = -ε · v(t_i)`) — la lave qui retombe et roule sur les flancs.
- **Lumière dynamique** : la position de la lumière suit le cratère, et son intensité oscille avec un bruit de Perlin 1D pour simuler les pulsations de l'éruption.
- **Onde de choc** : un anneau qui se propage à chaque "boum" déclenché par l'utilisateur.
- **Skybox** : une vraie boîte texturée pour le ciel (cours section sur les skybox) au lieu d'un background uniforme.
- **Brouillard volumétrique non uniforme** : plus dense en bas, qui s'enroule autour du volcan.
- **Toon shading** sur certains éléments pour un look stylisé.

---

## Récapitulatif des notions de cours mobilisées

| Notion | Où dans le cours | Où dans ton projet |
|--------|------------------|---------------------|
| Champ de hauteur paramétrique | TP modeling, section terrain | Phase 1 |
| Bruit de Perlin multi-octaves | Section 4.5, TP `07_texture/c_perlin_noise` | Phase 1 (relief), Phase 3 (lave animée) |
| Coordonnées UV et textures | TP `07_texture/a_uv_texture` | Phase 1, 5, 6 |
| Modélisation par primitives (cyl + cône) | TP modeling, section arbres | Phase 5 |
| Particules en chute libre | Section 4.1 | Phase 2 |
| Billboards (sphérique et cylindrique) | Section 4.2, TP `07_texture/b_billboards` | Phase 4, 6 |
| Transparence : blending + tri + depth mask | Section 4.3 | Phase 4 |
| Discard dans fragment shader | Section 4.3 (fin) | Phase 6 |
| Effet de fog | TP shader, fin | Phase 7 |
| Uniformes temporels | TP shader, section animation | Phase 3 (lava), Phase 4 (smoke) |
| Illumination de Phong (et désactivation pour émissif) | Section 2.x du cours | Partout (terrain/arbres) vs Phase 2/3 (lava) |
| Instanciation visuelle (même mesh, plusieurs translations) | Section 4.4, figure 21 | Phase 5 (arbres), Phase 6 (herbe) |

Tu vois que ton projet « cocher » à peu près toute la liste — c'est exactement ce qu'on attend d'un projet final.