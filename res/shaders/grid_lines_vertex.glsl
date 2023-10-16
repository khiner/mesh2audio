#version 330 core

uniform mat4 camera_view;
uniform mat4 projection;

layout(location = 0) in vec3 Pos;

out vec3 nearPoint;
out vec3 farPoint;
out mat4 fragView;
out mat4 fragProj;

vec3 UnprojectPoint(float x, float y, float z, mat4 viewInv, mat4 projInv) {
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    mat4 viewInv = inverse(camera_view);
    mat4 projInv = inverse(projection);
    nearPoint = UnprojectPoint(Pos.x, Pos.y, 0.0, viewInv, projInv).xyz; // unprojecting on the near plane
    farPoint = UnprojectPoint(Pos.x, Pos.y, 1.0, viewInv, projInv).xyz; // unprojecting on the far plane
    fragView = camera_view;
    fragProj = projection;
    gl_Position = vec4(Pos, 1.0);
}
