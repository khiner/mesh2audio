#version 330 core

// Supports per-instance 3D translation, uniform scaling, and color.
// Passes outputs straight to fragment shader (so no line-drawing support via geometry shader).

uniform mat4 camera_view;
uniform mat4 projection;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;
layout (location = 3) in vec4 aTranslateScale; // {x, y, z, scale}

out vec3 frag_in_normal;
out vec4 frag_in_position;
out vec4 frag_in_color;

void main() {
    frag_in_position = vec4(aPos + aTranslateScale.xyz, aTranslateScale.w);
    frag_in_normal = aNormal;
    frag_in_color = aColor;

    gl_Position = projection * camera_view * frag_in_position;
}
