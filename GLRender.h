#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

class GLRender
{
private:
	GLFWwindow* window;

public:
	
	void process_input(GLFWwindow* window);
	int window_init();
	void loop();
};
