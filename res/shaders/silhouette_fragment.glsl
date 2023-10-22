#version 330 core

out vec4 frag_color;

uniform vec4 line_color;

void main() {
    frag_color = line_color;
}
