#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
int window_init();