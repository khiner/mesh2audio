#version 330 core

// Following this guide: https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid

in vec3 nearPoint;
in vec3 farPoint;
in mat4 fragView;
in mat4 fragProj;
out vec4 outColor;

const float near = 0.1;
const float far = 100;

vec4 grid(vec3 frag_pos, float scale, bool drawAxis) {
    vec2 coord = frag_pos.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    // z axis
    if(frag_pos.x > -0.1 * minimumx && frag_pos.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(frag_pos.z > -0.1 * minimumz && frag_pos.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}
float computeDepth(vec3 pos) {
    vec4 clip_space_pos = fragProj * fragView * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}
float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = fragProj * fragView * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between near and far
    return linearDepth / far; // normalize
}
void main() {
    vec3 nf = farPoint - nearPoint;
    float t = -nearPoint.y / nf.y;
    vec3 frag_pos = nearPoint + t * nf;
    // xxx the `* 2` multiplier here differs from the guide, and should not bee needed. Should be able to use `frag_pos` here.
    // Also, this correction doesn't work in all view angles/distances. Something is wrong.
    vec3 frag_pos_2 = nearPoint + t * 2 * nf;
    gl_FragDepth = computeDepth(frag_pos_2);
    float linearDepth = computeLinearDepth(frag_pos_2);
    float fading = max(0, (0.5 - linearDepth));

    outColor = (grid(frag_pos, 10, true) + grid(frag_pos, 1, true)) * float(t > 0); // adding multiple resolution for the grid
    outColor.a *= fading;
}