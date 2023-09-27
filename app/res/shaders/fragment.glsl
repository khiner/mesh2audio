#version 330 core

const int num_lights = 5;

uniform mat4 camera_view;
uniform vec4 light_position[num_lights], light_color[num_lights];
uniform vec4 ambient_color, diffuse_color, specular_color;
uniform float shininess_factor;
uniform int flat_shading; // 0 for smooth shading, 1 for flat shading

in vec3 geom_vertex_normal;
in vec4 geom_vertex_position;
in vec4 geom_instance_color;

out vec4 frag_color;

vec4 compute_lighting(vec3 direction, vec4 light_color, vec3 normal, vec3 half_vector, vec4 object_diffuse, vec4 object_specular, float object_shininess) {
    float n_dot_l = dot(normal, direction);
    vec4 lambert = object_diffuse * light_color * max(n_dot_l, 0.0);
    vec4 phong = object_specular * light_color * pow(max(dot(normal, half_vector), 0.0), object_shininess);
    return lambert + phong;
}       

void main (void) {
    vec3 eye_position = vec3(0,0,0);
    vec3 fragment_position = geom_vertex_position.xyz / geom_vertex_position.w;
    vec3 eye_direction = normalize(eye_position - fragment_position);
    vec3 normal = normalize(flat_shading == 1 ? cross(dFdx(fragment_position), dFdy(fragment_position)) : geom_vertex_normal);

    vec4 final_color = ambient_color;
    for (int i = 0; i < num_lights; i++) {
        vec4 light_pos = inverse(camera_view) * light_position[i];
        vec3 pos = light_pos.xyz / light_pos.w;
        vec3 dir = normalize(pos - fragment_position);
        vec3 half_vector = normalize(dir + eye_direction);

        final_color += compute_lighting(dir, light_color[i], normal, half_vector, diffuse_color, specular_color, shininess_factor);
    }

    frag_color = final_color * geom_instance_color;
}
