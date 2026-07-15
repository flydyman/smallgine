#pragma once
#include "../platform/glcontext.hpp"
#include "../tools/shader.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace smallgine {

// Screen-space 2D rectangle for immediate-mode panels/buttons (pixel coords,
// origin top-left to match the text renderer).
struct Rect { float x, y, w, h; bool contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; } };

// A labeled clickable region emitted by the editor each frame.
struct UIButton { Rect rect; std::string label; int id; };

// Minimal colored-quad renderer for editor panels. Text drawn separately.
class UI {
private:
    GLuint prog = 0, vao = 0, vbo = 0;
    GLint uRect = -1, uScreen = -1, uColor = -1;

public:
    void init()
    {
        prog = tools::linkProgramFiles("assets/shaders/ui.vert", "assets/shaders/ui.frag");
        uRect = glGetUniformLocation(prog, "uRect");
        uScreen = glGetUniformLocation(prog, "uScreen");
        uColor = glGetUniformLocation(prog, "uColor");
        const float quad[12] = { 0,0, 1,0, 1,1, 1,1, 0,1, 0,0 };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // Draw one filled rectangle. Caller sets blend + disables depth/cull.
    void rect(const Rect& r, const glm::vec4& color, int screenW, int screenH)
    {
        glUseProgram(prog);
        glUniform4f(uRect, r.x, r.y, r.w, r.h);
        glUniform2f(uScreen, (float)screenW, (float)screenH);
        glUniform4fv(uColor, 1, &color[0]);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void free()
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
    }
};

} // namespace smallgine
