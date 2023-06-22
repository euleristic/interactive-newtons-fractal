#version 410 core

// This shader renders Newton's fractal.
// It does this by seeing what zero the seed value represented by the pixel approaches under Newton's method, coloring it accordingly.
// If the iterated value is not within the allowed error sqrt(TEMPLATE_EPSILON_SQR) of a zero after TEMPLATE_ITERATION_COUNT iterations,
// the pixel is left black.

in vec2 render_space_position;

out vec4 frag_color;

uniform dvec4 fractal_space_screen_rect; // Where xy is center and zw is half in positive x and positive y
uniform dvec2 coefficients[TEMPLATE_DEGREE + 1];
uniform dvec2 zeros[TEMPLATE_DEGREE];

const vec4 colors[TEMPLATE_DEGREE] = {
	TEMPLATE_COLOR_LIST
};

dvec2 conjugate(dvec2 z) {
	return dvec2(z.x, -z.y);
}

dvec2 complex_mul(dvec2 lhs, dvec2 rhs) {
	return dvec2(lhs.x * rhs.x - lhs.y * rhs.y, lhs.x * rhs.y + lhs.y * rhs.x);
}

dvec2 complex_div(dvec2 numerator, dvec2 denominator) {
	return complex_mul(numerator, conjugate(denominator)) / (denominator.x * denominator.x + denominator.y * denominator.y); 
}

dvec2 newton_iterate(dvec2 z) {
	dvec2 p = dvec2(0.0, 0.0);
	dvec2 p_prime = dvec2(0.0, 0.0);
	dvec2 accumulated_power = dvec2(1.0, 0.0);
	for (uint n = 0; n < TEMPLATE_DEGREE; ++n) {
		p += complex_mul(coefficients[n], accumulated_power);
		p_prime += (n + 1) * complex_mul(coefficients[n + 1], accumulated_power);
		accumulated_power = complex_mul(accumulated_power, z);
	}
	p += complex_mul(coefficients[TEMPLATE_DEGREE], accumulated_power);
	return z - complex_div(p, p_prime);
}

dvec2 fractal_space_point() {
	dvec2 type_casted = render_space_position;
	return fractal_space_screen_rect.xy + dvec2(fractal_space_screen_rect.z * type_casted.x, fractal_space_screen_rect.w * type_casted.y);
}



vec4 select_color(dvec2 z) {
	for (int i = 0; i < TEMPLATE_DEGREE; ++i) {
		dvec2 relative = zeros[i] - z;
		if (dot(relative, relative) < TEMPLATE_EPSILON_SQR) {
			return colors[i];
		}
	}
	return vec4(0.0);
}

void main() {
	dvec2 z = fractal_space_point();
	for (int i = 0; i < TEMPLATE_ITERATION_COUNT; ++i) {
		z = newton_iterate(z);
	}
	frag_color = select_color(z);
}