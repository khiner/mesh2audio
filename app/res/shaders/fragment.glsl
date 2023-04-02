# version 330 core

// Inputs passed in from the vertex shader.
in vec3 mynormal;
in vec4 myvertex;

// Output of the fragment shader.
out vec4 fragColor;

// Max number of light sources.
const int num_lights = 5;

// Uniform variable modelview.
uniform mat4 modelview;

// Uniform variables to do with lighting.
uniform vec4 light_posn[num_lights], light_col[num_lights];

// Uniform variable for object properties.
uniform vec4 ambient, diffuse, specular;
uniform float shininess;

vec4 compute_lighting(vec3 direction, vec4 lightcolor, vec3 normal, vec3 halfvec, vec4 mydiffuse, vec4 myspecular, float myshininess) {
    float n_dot_l = dot(normal, direction);
    vec4 lambert = mydiffuse * lightcolor * max(n_dot_l, 0.0);
    vec4 phong = myspecular * lightcolor * pow(max(dot(normal, halfvec), 0.0), myshininess);
    return lambert + phong;
}       

void main (void) {
    // Calculate camera direction
    vec3 eyepos = vec3(0,0,0);
    vec3 mypos = myvertex.xyz / myvertex.w;
    vec3 eyedirn = normalize(eyepos - mypos);

    // Normalise the normal at that point.
    vec3 normal = normalize(mynormal);

    // Start with ambient color, and iterate through each light source (on or off).
    vec4 finalcolor = vec4(ambient);
    for (int i = 0; i < num_lights; i++) {
        vec4 light_position = inverse(modelview) * light_posn[i];
        vec3 position = light_position.xyz / light_position.w;
        vec3 direction = normalize(position - mypos);
        vec3 half_i = normalize(direction + eyedirn);
        vec4 light_color = light_col[i];

        // Compute light based on the normal at the point and the light direction.
        finalcolor += compute_lighting(direction, light_color, normal, half_i, diffuse, specular, shininess);
    }

    fragColor = finalcolor;
}
