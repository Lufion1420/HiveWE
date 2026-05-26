#version 450 core

layout (location = 2) uniform vec4 selection_color;
layout (location = 3) uniform bool filled_mode;

out vec4 color;

in vec2 uv;

void main() {
	float R = 1.0;
	float R2 = 0.93;
	float dist = sqrt(dot(uv * 2.f - 1.f, uv * 2.f - 1.f));
	if (dist >= R) {
		discard;
	}

	if (!filled_mode) {
		if (dist <= R2) {
			discard;
		}
		color = selection_color;
		return;
	}

	vec4 fill = selection_color;
	fill.a *= 0.35;

	if (dist > R2) {
		color = selection_color;
	} else {
		color = fill;
	}
}
