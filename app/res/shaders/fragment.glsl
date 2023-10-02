#version 330 core

struct Light {
    vec4 position;
    vec4 color;
};

const int num_lights = 5;
layout (std140) uniform LightBlock {
    Light lights[num_lights];
};

uniform mat4 camera_view;
uniform vec4 ambient_color, diffuse_color, specular_color;
uniform float shininess_factor;
uniform int flat_shading; // 0 for smooth shading, 1 for flat shading

in vec4 frag_in_position;
in vec3 frag_in_normal;
in vec4 frag_in_color;

out vec4 frag_color;

vec4 compute_lighting(vec3 direction, vec4 light_color, vec3 normal, vec3 half_vector) {
    vec4 lambert = diffuse_color * light_color * max(dot(normal, direction), 0.0);
    vec4 phong = specular_color * light_color * pow(max(dot(normal, half_vector), 0.0), shininess_factor);
    return lambert + phong;
}       

void main (void) {
    vec3 fragment_position = frag_in_position.xyz / frag_in_position.w;
    vec3 eye_direction = normalize(-fragment_position);
    vec3 normal = normalize(flat_shading == 1 ? cross(dFdx(fragment_position), dFdy(fragment_position)) : frag_in_normal);
    mat4 inv_camera_view = inverse(camera_view);
    vec4 final_color = ambient_color;
    for (int i = 0; i < num_lights; i++) {
        vec4 light_pos = inv_camera_view * lights[i].position;
        vec3 pos = light_pos.xyz / light_pos.w;
        vec3 dir = normalize(pos - fragment_position);
        vec3 half_vector = normalize(dir + eye_direction);
        final_color += compute_lighting(dir, lights[i].color, normal, half_vector);
    }

    frag_color = final_color * frag_in_color;
}
