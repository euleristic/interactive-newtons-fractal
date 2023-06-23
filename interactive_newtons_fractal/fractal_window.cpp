#include "fractal_window.hpp"

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"

#include <string_view>
#include <fstream>
#include <exception>
#include <sstream>
#include <ranges>
#include <bit>
#include <span>
#include <cmath>
#include <filesystem>
#include <numbers>

#include <iostream>

using namespace std::string_literals;

namespace euleristic {

	// Necessarily global things, due to how GLFW callbacks work

	double scroll_delta{};
	double window_width, window_height;

	void resize_window(GLFWwindow* window, int width, int height) {
		window_width = static_cast<double>(width);
		window_height = static_cast<double>(height);
		glViewport(0, 0, width, height);
	}

	void mouse_scroll(GLFWwindow* window, double xoffset, double yoffset) {
		scroll_delta += yoffset;
	};

	// Utilities

	constexpr std::array<float, 12> normal_square_buffer = {
		-1.0f, 1.0f, // top left
		1.0f, 1.0f, // top right
		-1.0f, -1.0f, // bottom left

		1.0f, 1.0f, // top right
		1.0f, -1.0f, // bottom right
		-1.0f, -1.0f // bottom left
	};

	constexpr std::string replace_all_of(std::string_view source, std::string_view old_substring, std::string_view new_substring) {
		std::string destination{};
		auto cursor = source.begin();
		while (true) {
			auto substr = std::ranges::search(cursor, source.end(), old_substring.begin(), old_substring.end());
			destination += std::string(cursor, substr.begin());
			if (substr.begin() == substr.end()) break;
			destination += new_substring;
			cursor = substr.end();
		}
		return destination;
	}

	// This could be any arithmetic type, but for clarity only complex is used there
	constexpr std::vector<std::complex<double>> zeros_to_coefficients(std::span<std::complex<double>> zeros) {

		if (zeros.size() > 63) {
			throw std::invalid_argument("Size of zeros was greater than 63.");
		}

		std::vector coefs(zeros.size() + 1, std::complex<double>{});

		for (std::uint64_t bits = 0; bits < (1ui64 << zeros.size()); ++bits) {
			std::complex<double> product(1.0f, 0.0f);
			for (size_t i = 0; i < zeros.size(); ++i) {
				if ((bits >> i) & 1ui64) {
					product *= -zeros[i];
				}
			}
			coefs[zeros.size() - std::popcount(bits)] += product;
		}
		return coefs;
	}

	template<std::floating_point component_type>
	constexpr glm::mat<3, 3, component_type> translate(const glm::vec<2, component_type> by) {
		return glm::mat<3, 3, component_type>(
			glm::vec<3, component_type>(1.0, 0.0, 0.0),
			glm::vec<3, component_type>(0.0, 1.0, 0.0),
			glm::vec<3, component_type>(by.x, by.y, 1.0));
	}

	template<std::floating_point component_type>
	constexpr glm::mat<3, 3, component_type> scale(const glm::vec<2, component_type> by) {
		return glm::mat<3, 3, component_type>(
			glm::vec<3, component_type>(by.x, 0.0, 0.0),
			glm::vec<3, component_type>(0.0, by.y, 0.0),
			glm::vec<3, component_type>(0.0, 0.0, 1.0));
	}

	// So I don't need to add the homogeneous component so often
	template<size_t length, std::floating_point component_type>
	constexpr glm::vec<length, component_type> operator*(glm::mat<length + 1, length + 1, component_type> mat,
		glm::vec<length, component_type> vec) noexcept {
		return (mat * glm::vec<length + 1, component_type>(vec, 1.0)).xy;
	}

	std::string load_file(std::filesystem::path path) {
		std::ifstream input_file(path);
		if (!input_file.is_open()) {
			throw std::exception(("Could not load file at path: "s + path.string()).c_str());
		}
		std::stringstream stringstream;
		stringstream << input_file.rdbuf();
		input_file.close();
		return stringstream.str();
	}

	void compile_shader(const int shader_id, const char* source, const char* shader_name = "Unspecified") {
		glShaderSource(shader_id, 1, &source, nullptr);
		glCompileShader(shader_id);

		int success;
		glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
		if (!success) {
			char info_log[512];
			glGetShaderInfoLog(shader_id, 512, nullptr, info_log);
			throw graphics_error{ ("COMPILATION_ERROR in : "s + shader_name).c_str(), info_log };
		}
	}

	// Where h, s, v are in [0, 1]
	glm::vec4 hsv_to_rgba(float hue, float saturation, float value) {
		// Credit to Wikipedia: https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB

		float chroma = value * saturation;
		float x = chroma * (1.0f - std::fabsf(std::fmodf(hue * 6.0f, 2.0f) - 1.0f));

		glm::vec3 rgb1;
		if (hue <= 1.0 / 6.0)
			rgb1 = glm::vec3(chroma, x, 0.0f);
		else if (hue <= 2.0 / 6.0)
			rgb1 = glm::vec3(x, chroma, 0.0f);
		else if (hue <= 3.0 / 6.0)
			rgb1 = glm::vec3(0.0f, chroma, x);
		else if (hue <= 4.0 / 6.0)
			rgb1 = glm::vec3(0.0f, x, chroma);
		else if (hue <= 5.0 / 6.0)
			rgb1 = glm::vec3(x, 0.0f, chroma);
		else
			rgb1 = glm::vec3(chroma, 0.0f, x);

		float m = value - chroma;

		return glm::vec4(rgb1.r + m, rgb1.g + m, rgb1.b + m, 1.0f);
	}

	glm::vec4 generate_color(const size_t index) {
		constexpr float saturation = 1.0f;
		constexpr float value = 0.5f;
		// I'm sure I'm not the first to think of this, but the palette is basically selected by walking phi 
		// circumferences around the color wheel from the previous one. Since phi is the "most irrational number",
		// this method generates the an optimally uniform distribution of colors as the size of the palette approaches infinity.
		return hsv_to_rgba(fmodf((float)index * std::numbers::phi_v<float>, 1.0), saturation, value);
	}

	std::string generate_color_list_code(size_t count) {
		std::string code;
		for (size_t i = 0; i < count; ++i) {
			auto color = generate_color(i);
			code += "vec4(" + std::to_string(color.r) + ", " + std::to_string(color.g) + ", "
				+ std::to_string(color.b) + ", " + std::to_string(color.a) + ')';
			if (i + 1 < count) {
				code += ",\n\t";
			}
		}
		return code;
	}

	constexpr const char* reflect_GL_error(GLenum code) {
		switch (code) {
		case 0: return "No error";
		case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
		case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
		case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
		case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_CONTEXT_LOST: return "GL_CONTEXT_LOST";
		default: return "UNKNOWN GL ERROR CODE";
		}
	}

	constexpr const char* reflect_GLFW_error(int code) {
		switch (code) {
		case 0: return "No error";
		case GLFW_NOT_INITIALIZED: return "GLFW_NOT_INITIALIZED";
		case GLFW_NO_CURRENT_CONTEXT: return "GLFW_NO_CURRENT_CONTEXT";
		case GLFW_INVALID_ENUM: return "GLFW_INVALID_ENUM";
		case GLFW_INVALID_VALUE: return "GLFW_INVALID_VALUE";
		case GLFW_OUT_OF_MEMORY: return "GLFW_OUT_OF_MEMORY";
		case GLFW_API_UNAVAILABLE: return "GLFW_API_UNAVAILABLE";
		case GLFW_VERSION_UNAVAILABLE: return "GLFW_VERSION_UNAVAILABLE";
		case GLFW_PLATFORM_ERROR: return "GLFW_PLATFORM_ERROR";
		case GLFW_FORMAT_UNAVAILABLE: return "GLFW_FORMAT_UNAVAILABLE";
		default: return "UNKNOWN GLFW ERROR CODE";
		}
	}

	// Constants

	constexpr double initial_pixels_per_unit = 200.0; //This being double saves a cast.
	constexpr double zoom_rate = 1.1;
	constexpr float zero_total_radius = 10.0f;
	constexpr float zero_total_radius_sqr = zero_total_radius * zero_total_radius;
	constexpr float zero_inner_radius_ratio = 0.8f;
	constexpr auto zero_scale = scale(glm::vec2(zero_total_radius, zero_total_radius));

	// Fractal Window

	void fractal_window::recompile_shaders() {

		// Complete fractal shader

		std::string source = replace_all_of(fractal_fragment_shader_source_template, "TEMPLATE_DEGREE", std::to_string(zeros.size()));
		source = replace_all_of(source, "TEMPLATE_ITERATION_COUNT", std::to_string(iteration_count));
		source = replace_all_of(source, "TEMPLATE_EPSILON_SQR", std::to_string(epsilon_squared));
		source = replace_all_of(source, "TEMPLATE_COLOR_LIST", generate_color_list_code(zeros.size()).c_str());

		compile_shader(fractal_fragment_shader, source.c_str(), "Fractal Fragment Shader");

		glAttachShader(fractal_shader_program, fractal_vertex_shader);
		glAttachShader(fractal_shader_program, fractal_fragment_shader);
		glLinkProgram(fractal_shader_program);

		int success;
		glGetProgramiv(fractal_shader_program, GL_LINK_STATUS, &success);
		if (!success) {
			char info_log[512];
			glGetProgramInfoLog(fractal_shader_program, 512, nullptr, info_log);
			throw graphics_error{ "SHADER_LINKING_ERROR", info_log };
		}

		// Complete zeros shader

		source = replace_all_of(zeros_fragment_shader_source_template, "TEMPLATE_RADIUS_SQR", std::to_string(zero_inner_radius_ratio));

		compile_shader(zeros_fragment_shader, source.c_str(), "Zeros Fragment Shader");

		glAttachShader(zeros_shader_program, zeros_vertex_shader);
		glAttachShader(zeros_shader_program, zeros_fragment_shader);
		glLinkProgram(zeros_shader_program);

		glGetProgramiv(zeros_shader_program, GL_LINK_STATUS, &success);
		if (!success) {
			char info_log[512];
			glGetProgramInfoLog(zeros_shader_program, 512, nullptr, info_log);
			throw graphics_error{ "SHADER_LINKING_ERROR", info_log };
		}

		// Get uniforms

		fractal_screen_rect_uniform = glGetUniformLocation(fractal_shader_program, "fractal_space_screen_rect");
		fractal_coefficients_uniform = glGetUniformLocation(fractal_shader_program, "coefficients");
		fractal_zeros_uniform = glGetUniformLocation(fractal_shader_program, "zeros");

		zeros_transform_uniform = glGetUniformLocation(zeros_shader_program, "transform");
		zeros_color_uniform = glGetUniformLocation(zeros_shader_program, "color");
	}

	void fractal_window::add_zero(const std::complex<double> zero) noexcept {
		zeros.push_back(zero);
		recompile_shaders();
		coefficients = zeros_to_coefficients(zeros);
	}

	void fractal_window::remove_zero(const size_t index) {
		zeros.erase(zeros.cbegin() + index);
		recompile_shaders();
		coefficients = zeros_to_coefficients(zeros);
	}

	fractal_window::fractal_window(int width, int height) {
		window_width = width;
		window_height = height;

		fractal_to_screen_space = translate(glm::dvec2(0.5 * window_width, 0.5 * window_height))
			* scale(glm::dvec2(initial_pixels_per_unit, -initial_pixels_per_unit));

		screen_to_fractal_space = glm::inverse(fractal_to_screen_space);

		zeros.push_back({ -1.0, 0.0 });
		zeros.push_back({ 0.5, sinf(glm::pi<float>() / 3) });
		zeros.push_back({ 0.5, -sinf(glm::pi<float>() / 3) });

		if (!glfwInit()) {
			const char* message_ptr;
			auto code = glfwGetError(&message_ptr);
			throw graphics_error{ reflect_GLFW_error(code), message_ptr};
		};

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(width, height, "", nullptr, nullptr);

		if (!window) {
			throw graphics_error{ reflect_GL_error(glGetError()), "Failed to create window."};
		}

		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			throw graphics_error{ "No code", "Failed to initialize GLAD."};
		}

		glViewport(0, 0, width, height);

		// Set callbacks
		glfwSetWindowSizeCallback(window, resize_window);
		glfwSetScrollCallback(window, mouse_scroll);

		// Shaders
		fractal_shader_program = glCreateProgram();
		zeros_shader_program = glCreateProgram();

		// Load, compile and link shaders

		fractal_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		compile_shader(fractal_vertex_shader, load_file("fractal_vertex_shader.glsl").c_str(), "Fractal Vertex Shader");
		zeros_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		compile_shader(zeros_vertex_shader, load_file("zeros_vertex_shader.glsl").c_str(), "Zeros Vertex Shader");

		fractal_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		fractal_fragment_shader_source_template = load_file("fractal_fragment_shader_template.glsl");
		zeros_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		zeros_fragment_shader_source_template = load_file("zeros_fragment_shader_template.glsl");

		// Complete and compile shader templates, and link shaders

		recompile_shaders();

		// Square vertex array (the same for both shader, nice)

		glGenVertexArrays(1, &normal_square_vao);
		glBindVertexArray(normal_square_vao);
		glEnableVertexAttribArray(0);

		glGenBuffers(1, &normal_square_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, normal_square_vbo);
		glBufferData(GL_ARRAY_BUFFER, normal_square_buffer.size() * sizeof(float), normal_square_buffer.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, false, 2 * sizeof(float), reinterpret_cast<void*>(0));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		coefficients = zeros_to_coefficients(zeros);

		// Enable alpha blending 

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	void fractal_window::set_title(const char* title) noexcept {
		glfwSetWindowTitle(window, title);
	}

	void fractal_window::handle_scroll_wheel() noexcept {
		if (scroll_delta == 0.0) return;
		auto mouse_pos = mouse_position();
		auto scale_factor = std::pow(zoom_rate, -scroll_delta);

		glm::dmat3 delta(
			scale_factor, 0.0, 0.0,
			0.0, scale_factor, 0.0,
			(1.0 - scale_factor) * (mouse_pos.x * screen_to_fractal_space[0][0] + screen_to_fractal_space[2][0]),
			(1.0 - scale_factor) * (mouse_pos.y * screen_to_fractal_space[1][1] + screen_to_fractal_space[2][1]),
			1.0);

		screen_to_fractal_space = delta * screen_to_fractal_space;
		fractal_to_screen_space = glm::inverse(screen_to_fractal_space);
		scroll_delta = 0.0;
	}

	void fractal_window::handle_mouse_buttons() noexcept {
		// Left button

		bool current_left_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
		auto current_mouse_pos = mouse_position();

		// Did the press begin this frame?
		if (!last_left_mouse_button_state && current_left_state) {
			for (size_t i = 0; i < zeros.size(); ++i) {
				if (glm::length2(current_mouse_pos - fractal_to_screen_space * glm::dvec2(zeros[i].real(), zeros[i].imag())) < zero_total_radius_sqr) {
					held_zero = i;
					break;
				}
			}
		}
		// Is the press continuing?
		else if (last_left_mouse_button_state && current_left_state) {
			if (held_zero) {
				auto zero = screen_to_fractal_space * current_mouse_pos;
				zeros[*held_zero] = std::complex(zero.x, zero.y);
				coefficients = zeros_to_coefficients(zeros);
			}
			else {
				fractal_to_screen_space = translate(current_mouse_pos - last_mouse_pos) * fractal_to_screen_space;
				screen_to_fractal_space = glm::inverse(fractal_to_screen_space);
			}
		}
		// Was the button released?
		else if (last_left_mouse_button_state && !current_left_state) {
			held_zero = {};
		}
		last_mouse_pos = current_mouse_pos;
		last_left_mouse_button_state = current_left_state;

		// Right button
		bool current_right_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS;

		// Did the press begin this frame?
		if (!last_right_mouse_button_state && current_right_state) {
			for (size_t i = 0; i < zeros.size(); ++i) {
				if (glm::length2(current_mouse_pos - fractal_to_screen_space * glm::dvec2(zeros[i].real(), zeros[i].imag())) < zero_total_radius_sqr) {
					zero_to_remove = i;
					break;
				}
			}
		}
		// Was the button released?
		else if (last_right_mouse_button_state && !current_right_state) {
			if (zero_to_remove) {
				for (size_t i = 0; i < zeros.size(); ++i) {
					if (glm::length2(current_mouse_pos - fractal_to_screen_space * glm::dvec2(zeros[i].real(), zeros[i].imag())) < zero_total_radius_sqr) {
						if (zero_to_remove == i) {
							remove_zero(i);
							break;
						}
					}
				}
			}
			else {
				auto zero = screen_to_fractal_space * current_mouse_pos;
				add_zero(std::complex(zero.x, zero.y));
			}
			zero_to_remove = {};
		}

		last_right_mouse_button_state = current_right_state;
	}

	bool fractal_window::should_close() const noexcept {
		return glfwWindowShouldClose(window);
	}

	void fractal_window::render() noexcept {

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindVertexArray(normal_square_vao);
		glUseProgram(fractal_shader_program);
		auto center = screen_to_fractal_space * glm::dvec2(window_width / 2.0, window_height / 2.0);
		auto half = screen_to_fractal_space * glm::dvec2(window_width, 0.0) - center;
		glUniform4d(fractal_screen_rect_uniform, center.x, center.y, half.x, half.y);

		// The C++ standard guarantees that std::complex is castable in this manner, independent of implementation! :D
		glUniform2dv(fractal_coefficients_uniform, coefficients.size(), reinterpret_cast<GLdouble*>(coefficients.data()));
		glUniform2dv(fractal_zeros_uniform, zeros.size(), reinterpret_cast<GLdouble*>(zeros.data()));
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glm::mat3 screen_to_normal_space = translate(glm::vec2(-1.0, 1.0)) * scale(glm::vec2(2.0 / window_width, -2.0 / window_height));

		auto zero = glm::vec2(zeros[0].real(), zeros[0].imag());
		auto trans = translate(glm::mat3(fractal_to_screen_space) * zero);
		auto normal_pos = screen_to_normal_space * trans * glm::vec2(0.0);

		glUseProgram(zeros_shader_program);
		for (size_t i = 0; i < zeros.size(); ++i) {
			auto transform = screen_to_normal_space * translate(glm::mat3(fractal_to_screen_space) * glm::vec2(zeros[i].real(), zeros[i].imag()))
				* zero_scale;
			glUniformMatrix3fv(zeros_transform_uniform, 1, GL_FALSE, glm::value_ptr(glm::mat3(transform)));
			auto color = generate_color(i);
			glUniform4f(zeros_color_uniform, color.r, color.g, color.b, color.a);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		glBindVertexArray(0);
		glfwSwapBuffers(window);

	}

	void fractal_window::poll_events() {
		glfwPollEvents();
	}

	glm::dvec2 fractal_window::mouse_position() const noexcept {
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		return { x, y };
	}

	fractal_window::~fractal_window() noexcept {
		glDeleteBuffers(1, &normal_square_vbo);
		glDeleteVertexArrays(1, &normal_square_vao);
		glDeleteShader(zeros_fragment_shader);
		glDeleteShader(fractal_fragment_shader);
		glDeleteShader(zeros_vertex_shader);
		glDeleteShader(fractal_vertex_shader);
		glDeleteProgram(zeros_shader_program);
		glDeleteProgram(fractal_shader_program);
		glfwDestroyWindow(window);
		glfwTerminate();
	}
}