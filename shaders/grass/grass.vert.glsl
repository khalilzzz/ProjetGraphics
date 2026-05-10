#version 330 core

layout (location = 0) in vec3 vertex_position;
layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec3 vertex_color;
layout (location = 3) in vec2 vertex_uv;

out struct fragment_data {
    vec3 position;
    vec3 normal;
    vec3 color;
    vec2 uv;
} fragment;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 position = model * vec4(vertex_position, 1.0);
    mat4 modelNormal = transpose(inverse(model));
    vec4 normal = modelNormal * vec4(vertex_normal, 0.0);

    fragment.position = position.xyz;
    fragment.normal   = normal.xyz;
    fragment.color    = vertex_color;
    fragment.uv       = vertex_uv;

    gl_Position = projection * view * position;
}
