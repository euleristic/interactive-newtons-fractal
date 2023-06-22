#version 410 core

layout (location = 0) in vec2 render_space_position_attribute;

out vec2 render_space_position;

void main() {
	gl_Position = vec4(render_space_position_attribute, 0.0, 1.0);
	render_space_position = render_space_position_attribute;
}