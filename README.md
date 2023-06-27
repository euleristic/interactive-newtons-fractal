# interactive_newtons_fractal

## Dependencies
* GLFW
* GLAD
* GLM

## Description
This app dynamically draws the Newton's fractal from the set of zeros of a complex polynomial. It is inspired by one such demonstration by Grant Sanderson (3Blue1Brown), [here](https://www.youtube.com/watch?v=-RdOwhmqP5s). To do this efficiently, the fractal is calculated on the GPU using OpenGL.

### What is this, anyway?
Newton's method is a way of finding a zero of a function `f`. This is done by first submitting a value `z` in the domain of `f` as an approximation of a zero (which may be a bad approximation). Then, adjust your approximation to be the zero of the linear function that is tangent to `f` at `z`. Iterate until satisfied. Numerically (in code, rather), the iteration simply looks like this: `z = z - f(z) / f_prime(z)`, where `f_prime` is the derivative of `f`.

Newton's fractal for a complex function `g` sorts the domain of `g` into the subsets which upon iteration under Newton's method, converge on a given zero. By assigning each zero and its corresponding basin of attraction a unique color, a beautiful and intricate pattern is visualized.

### Some interesting stuff
There are some interesting solutions in this code. To begin with, I'm quite pleased to only have used one vertex array object for the whole application, a normal square. This square is shaded both as the entire screen, which requires no transformation at all in the vertex shader, and as the zero representing disks. I used the OpenGL interface in quite a direct manner, with minimal wrapping, which is not the most pleasent way of interacting with OpenGL, but works nicely for small projects such as this.

In order to change the number of zeros in execution time, the sizes of the shader attributes and uniforms need to change. Thus, the fractal shaders use "templates" and are rewritten, recompiled and relinked when the zero count changes.

I realized an interesting algorithm for calculating the coefficients of a polynomial from the set of its zeros. I'm sure this is well known, but I always like finding things out for myself! First we express the polynomial as the product `(z - zeros[0]) * (z - zeros[1]) * ... *  (z - zeros[degree - 1])`, which multiplies out to the sum of all possible products where the left or right term in each factor is chosen. This can be represented as a binary number of a degree number of bits. So a bit of iteration, a bit manipulation, et voila!

Another interesting thing I did in this code was the color generation formula, which works by walking around a color wheel a certain angle. For each next color, an extra phi (golden ratio) of the circumference is walked around the wheel, since it's the "[most irrational number](https://www.youtube.com/watch?v=sj8Sg8qnjOg)". This is akin to how a sunflower distributes its seeds.

## How to use
Left click and drag a zero to translate it. Left click elsewhere and drag to translate the factal space and scroll to zoom in or out from a fracal space point. Right click a zero to remove it, right click elsewhere to add a zero at that point.