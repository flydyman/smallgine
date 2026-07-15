#pragma once
#include "glcontext.hpp"
#include <iostream>
#include <functional>

#include "../core/engine.hpp"

namespace smallgine {

static void error_callback(int error, const char* description)
{
    std::cout << "GLFW error: " << error << ": " << description << std::endl;
}

static void key_callback(GLFWwindow* /*window*/, int key, int scancode, int action, int mods){
    // Full control to engine
    engine.input(key, scancode, action, mods);
}

static void cursor_callback(GLFWwindow* /*window*/, double xpos, double ypos){
    engine.mouse(xpos, ypos);
}

static void scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset){
    engine.scroll(yoffset);
}

static void mouse_button_callback(GLFWwindow* /*window*/, int button, int action, int /*mods*/){
    engine.click(button, action);
}

static void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
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
        glfwWindowHint(GLFW_DEPTH_BITS, 24);

        window = glfwCreateWindow(w, h, title, NULL, NULL);
        if (window == NULL)
        {
            std::cout << "Failed to create GLFW3 window" << std::endl;
            glfwTerminate();
            return 2;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // vsync: cap loop to display refresh

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);

        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetKeyCallback(window, key_callback);
        glfwSetCursorPosCallback(window, cursor_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        return 0;
    }

    void run()
    {
        double time = glfwGetTime();
        engine.setWindow(window);
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

} // namespace smallgine