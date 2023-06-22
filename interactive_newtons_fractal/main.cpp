// Requires C++23 (because I use ranges::to once), GLFW, GLAD and GLM

#include <vector>
#include <iostream>
#include <format>
#include "fractal_window.hpp"

auto main() -> int {
	try {
		euleristic::fractal_window fractal(800, 600);
		auto last_frame = std::chrono::steady_clock::now();
		std::chrono::duration<double, std::milli> delta_time{};
		while (!fractal.should_close()) {
			
			fractal.update_zoom();
			fractal.update_pan();
			fractal.render();
			fractal.poll_events();

			fractal.set_title(std::format("Newton's Fractal! # of iterations: {}. Epsilon squared: {}. Frame duration: {:.5}ms.",
				fractal.iteration_count, fractal.epsilon_squared, delta_time.count()).c_str());
			delta_time = std::chrono::steady_clock::now() - last_frame;
			last_frame = std::chrono::steady_clock::now();
		}
	} catch (euleristic::graphics_error err) {
		std::cerr << "A fatal error was encountered. Code: " << err.code_name << ". " << err.message << '\n';
	} catch (std::exception& err) {
		std::cerr << err.what();
	} catch (...) {
		std::cerr << "Unknown error";
	}
	return 0;
}