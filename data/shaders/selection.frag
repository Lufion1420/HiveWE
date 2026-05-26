#version 450 core

layout (location = 2) uniform vec4 selection_color;

out vec4 color;

void main() {
	color = selection_color;
}
