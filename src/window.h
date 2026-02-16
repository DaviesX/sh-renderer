#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <optional>
#include <string_view>

namespace sh_renderer {

using Window = GLFWwindow *;

/*
 * @brief Creates a new window with an OpenGL 4.6 Core context.
 * @param width The width of the window.
 * @param height The height of the window.
 * @param title The title of the window.
 * @param msaa_samples Number of MSAA samples (0 to disable).
 * @return An optional containing the window if successful, std::nullopt
 * otherwise.
 */
std::optional<Window> CreateWindow(unsigned width, unsigned height,
                                   std::string_view title,
                                   unsigned msaa_samples = 0);

/*
 * @brief Destroys the given window.
 * @param window The window to destroy.
 */
void DestroyWindow(Window window);

}  // namespace sh_renderer
