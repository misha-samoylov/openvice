#ifndef GL_RENDER_H
#define GL_RENDER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>

static GLFWwindow* window;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void process_input(GLFWwindow* window);
int window_init();
void loop();

#endif