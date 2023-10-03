#version 330 core

uniform sampler2D shadow_map;
uniform int num_lights;
uniform vec4 ambient_color, diffuse_color, specular_color;
uniform float shininess_factor;
uniform int flat_shading; // 0 for smooth shading, 1 for flat shading

const int max_num_lights = 5; // Must be a constant. (Can't be a uniform.)
struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    mat4 view_projection;
};
layout (std140) uniform LightBlock {
    Light lights[max_num_lights];
};

in vec4 frag_in_position;
in vec3 frag_in_normal;
in vec4 frag_in_color;

out vec4 frag_color;

float get_shadow_factor(vec4 frag_pos_light_space, sampler2D depthMap) {
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    proj_coords = clamp(proj_coords, 0.0, 1.0);

    // todo provide contols over these values and experiment.
    float bias = 0.001;
    float shadow = 0.0;
    const int num_samples = 10;
    const float sample_range = 0.0006; // In texture space.
    for(int i = -num_samples / 2; i < num_samples / 2; ++i) {
        for(int j = -num_samples / 2; j < num_samples / 2; ++j) {
            float x = proj_coords.x + float(i) * sample_range;
            float y = proj_coords.y + float(j) * sample_range;
            if(x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0) {
                float closest_depth = texture(depthMap, vec2(x, y)).r;
                shadow += (proj_coords.z > closest_depth + bias) ? 1.0 : 0.0;
            }
        }
    }
    return shadow / float(num_samples * num_samples);
}

vec4 compute_lighting(vec3 direction, vec4 light_color, vec3 normal, vec3 half_vector) {
    vec4 lambert = diffuse_color * light_color * max(dot(normal, direction), 0.0);
    vec4 phong = specular_color * light_color * pow(max(dot(normal, half_vector), 0.0), shininess_factor);
    return lambert + phong;
}       

void main (void) {
    vec3 fragment_position = frag_in_position.xyz / frag_in_position.w;
    vec3 eye_direction = normalize(-fragment_position);
    vec3 normal = normalize(flat_shading == 1 ? cross(dFdx(fragment_position), dFdy(fragment_position)) : frag_in_normal);
    vec4 final_color = ambient_color;
    for (int i = 0; i < num_lights; i++) {
        vec4 light_pos = lights[i].position;
        vec3 pos = light_pos.xyz / light_pos.w;
        vec3 dir = normalize(pos - fragment_position);
        vec3 half_vector = normalize(dir + eye_direction);
        final_color += compute_lighting(dir, lights[i].color, normal, half_vector);
    }

    // Calculate shadow factor
    vec4 frag_pos_light_space = lights[0].view_projection * frag_in_position;
    float shadow_factor = get_shadow_factor(frag_pos_light_space, shadow_map);
    frag_color = (1.0 - shadow_factor) * final_color * frag_in_color;
}