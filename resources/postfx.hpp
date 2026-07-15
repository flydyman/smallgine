#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <iostream>

namespace smallgine {

namespace {
    const char* kPostVert =
        "#version 310 es\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
        "    vUV = p;\n"
        "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";

    const char* kPostFrag =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uScene;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    vec3 c = texture(uScene, vUV).rgb;\n"
        "    c = pow(c, vec3(0.9));\n"                       // gamma lift
        "    float d = distance(vUV, vec2(0.5));\n"
        "    float vig = smoothstep(0.85, 0.35, d);\n"       // vignette
        "    c *= mix(0.45, 1.0, vig);\n"
        "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
        "    c = mix(vec3(l), c, 1.18);\n"                   // saturation
        "    FragColor = vec4(c, 1.0);\n"
        "}\n";
}

// Offscreen render target + fullscreen post-processing pass.
class PostFX {
private:
    GLuint fbo = 0, colorTex = 0, depthRbo = 0, prog = 0, vao = 0;
    GLint uScene = -1;
    int w = 0, h = 0;

    void allocTargets(int width, int height)
    {
        w = width; h = height;
        if (!colorTex) glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (!depthRbo) glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

        if (!fbo) glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "PostFX FBO incomplete" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

public:
    void init(int width, int height)
    {
        prog = tools::linkProgram(kPostVert, kPostFrag);
        uScene = glGetUniformLocation(prog, "uScene");
        glGenVertexArrays(1, &vao); // empty VAO; verts from gl_VertexID
        allocTargets(width, height);
    }

    void resize(int width, int height)
    {
        if (width == w && height == h) return;
        allocTargets(width, height);
    }

    // Direct rendering into the offscreen target.
    void bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
    }

    // Resolve the offscreen color to the default framebuffer.
    void draw(int screenW, int screenH)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenW, screenH);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glUniform1i(uScene, 0);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }

    void free()
    {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (colorTex) glDeleteTextures(1, &colorTex);
        if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
