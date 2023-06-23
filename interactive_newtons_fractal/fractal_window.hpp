#include <vector>
#include <array>
#include <string>
#include <complex>
#include <chrono>
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

struct GLFWwindow;

namespace euleristic {

	struct graphics_error {
		const char* code_name, * message;
	};

	// This class encapsulates an OpenGL context, manages the visuals and handles devices input.
	class fractal_window {
		GLFWwindow* window;

		std::vector<std::complex<double>> coefficients;
		std::vector<std::complex<double>> zeros;
		std::vector<std::byte> zeros_vertex_buffer;

		std::string fractal_fragment_shader_source_template{};
		std::string zeros_fragment_shader_source_template{};

		unsigned int normal_square_vbo{};
		unsigned int normal_square_vao{};

		unsigned int fractal_vertex_shader{};
		unsigned int fractal_fragment_shader{};
		unsigned int fractal_shader_program{};
		unsigned int zeros_vertex_shader{};
		unsigned int zeros_fragment_shader{};
		unsigned int zeros_shader_program{};
		unsigned int fractal_screen_rect_uniform{};
		unsigned int fractal_coefficients_uniform{};
		unsigned int fractal_zeros_uniform{};
		unsigned int zeros_transform_uniform{};
		unsigned int zeros_color_uniform{};

		glm::dmat3 fractal_to_screen_space;
		glm::dmat3 screen_to_fractal_space;

		glm::dvec2 last_mouse_pos;
		std::optional<size_t> held_zero{};
		std::optional<size_t> zero_to_remove{};
		bool last_left_mouse_button_state{};
		bool last_right_mouse_button_state{};

		void recompile_shaders();
		void add_zero(const std::complex<double> zero) noexcept;
		void remove_zero(const size_t index);

	public:
		void handle_scroll_wheel() noexcept;
		void handle_mouse_buttons() noexcept;
		double epsilon_squared = 0.1;
		unsigned iteration_count = 20;

		fractal_window(int width, int height);
		void set_title(const char* title) noexcept;
		void render() noexcept;
		bool should_close() const noexcept;
		void poll_events();
		glm::dvec2 mouse_position() const noexcept;

		fractal_window(const fractal_window&) = default;
		fractal_window& operator=(const fractal_window&) = default;
		fractal_window(fractal_window&&) = default;
		fractal_window& operator=(fractal_window&&) = default;
		~fractal_window() noexcept;
	};
}