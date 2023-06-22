#version 410 core

layout (location = 0) in vec2 square_coordinate_attribute;

out vec2 square_coords;

uniform mat3 transform;

void main() {
	gl_Position = vec4((transform * vec3(square_coordinate_attribute, 1.0f)).xy, 0.0f, 1.0f);
	square_coords = square_coordinate_attribute;
}