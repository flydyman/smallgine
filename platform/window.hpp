#pragma once
#include <GLFW/glfw3.h>
#include <iostream>
#include <functional>

#include "../core/engine.hpp"

static void error_callback(int error, const char* description)
{
    std::cout << "GLFW error: " << error << ": " << description << std::endl;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    // Full control to engine
    engine.input(key, scancode, action, mods);
}

class GLWindow {
private:
    GLFWwindow* window;
public:
    int init(int w, int h, const char* title)
    {
        glfwSetErrorCallback(error_callback);
        if(!glfwInit())
        {
            return -1;
        }
        // GLES
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);

        window = glfwCreateWindow(w, h, title, NULL, NULL);
        if (window == NULL)
        {
            std::cout << "Failed to create GLFW3 window" << std::endl;
            glfwTerminate();
            return 2;
        }
        glfwMakeContextCurrent(window);
        // glfwSetFramebufferSizeCallback(window, )
        glfwSetKeyCallback(window, key_callback);
        return 0;
    }

    void run()
    {
        double time = glfwGetTime();
        engine.create();
        while (!glfwWindowShouldClose(window) && engine.looped())
        {
            double now = glfwGetTime();
            double dt = now - time;
            time = now;
            engine.process(dt);
            engine.draw(window);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        engine.cleanup();
        glfwTerminate();
    }
};