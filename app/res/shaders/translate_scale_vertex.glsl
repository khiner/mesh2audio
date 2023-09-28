#version 330 core

// Supports per-instance 3D translation, uniform scaling, and color.
// Passes outputs straight to fragment shader (so no line-drawing support via geometry shader).

uniform mat4 camera_view;
uniform mat4 projection;

layout (location = 0) in vec3 Pos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec4 Color;
layout (location = 3) in vec4 TranslateScale; // {x, y, z, scale}

out vec3 frag_in_normal;
out vec4 frag_in_position;
out vec4 frag_in_color;

void main() {
    frag_in_position = vec4(Pos + TranslateScale.xyz, TranslateScale.w);
    frag_in_normal = Normal;
    frag_in_color = Color;

    gl_Position = projection * camera_view * frag_in_position;
}
