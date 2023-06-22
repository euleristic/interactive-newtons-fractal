#version 410 core

// This shader draws a disk with a border. The disk with the border has unit radius, the "inner disk" has sqrt(TEMPLATE_RADIUS_SQR).

in vec2 square_coords;
out vec4 frag_color;

uniform vec4 color;

void main() {
	float coord_magnitude_sqr = dot(square_coords, square_coords);
	if (coord_magnitude_sqr > 1.0f)
		frag_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	else if (coord_magnitude_sqr > TEMPLATE_RADIUS_SQR)
		frag_color = vec4(1.0f);
	else
		frag_color = color;
}