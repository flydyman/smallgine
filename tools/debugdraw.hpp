#pragma once
#include "../platform/glcontext.hpp"
#include "shader.hpp"
#include "broadphase.hpp" // AABB
#include <glm/glm.hpp>
#include <vector>

namespace smallgine {

// Minimal immediate-mode line renderer for debug overlays (collider boxes, etc.).
// Accumulate lines/boxes, then flush() uploads and draws them in one GL_LINES
// call with a self-contained colored-line shader. Lazily initialized.
class DebugLines {
    GLuint prog = 0, vao = 0, vbo = 0;
    GLint  uVP = -1;
    std::vector<float> verts; // interleaved pos(3) + color(3) per vertex

public:
    void init()
    {
        if (prog) return;
        const char* vs =
            "#version 310 es\n"
            "layout(location=0) in vec3 aPos;\n"
            "layout(location=1) in vec3 aCol;\n"
            "uniform mat4 uVP; out vec3 vCol;\n"
            "void main(){ vCol = aCol; gl_Position = uVP * vec4(aPos, 1.0); }\n";
        const char* fs =
            "#version 310 es\n"
            "precision mediump float;\n"
            "in vec3 vCol; out vec4 frag;\n"
            "void main(){ frag = vec4(vCol, 1.0); }\n";
        prog = tools::linkProgram(vs, fs);
        uVP = glGetUniformLocation(prog, "uVP");
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    void clear() { verts.clear(); }
    bool empty() const { return verts.empty(); }

    void line(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        verts.insert(verts.end(), { a.x, a.y, a.z, c.r, c.g, c.b,
                                    b.x, b.y, b.z, c.r, c.g, c.b });
    }

    // The 12 edges of an axis-aligned box.
    void box(const AABB& bx, const glm::vec3& c)
    {
        const glm::vec3& mn = bx.min; const glm::vec3& mx = bx.max;
        glm::vec3 v[8] = {
            {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
            {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z} };
        static const int e[12][2] = {
            {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
        for (auto& pr : e) line(v[pr[0]], v[pr[1]], c);
    }

    // An oriented box: center, two horizontal unit axes (ax = along len, az =
    // along wid) and half-extents. Draws the 12 edges. Used for colliders that
    // rotate with their object (e.g. a car footprint).
    void obb(const glm::vec3& center, const glm::vec3& ax, const glm::vec3& az,
             float halfLen, float halfWid, float halfHt, const glm::vec3& c)
    {
        glm::vec3 L = ax * halfLen, W = az * halfWid, H = glm::vec3(0.0f, halfHt, 0.0f);
        glm::vec3 v[8];
        int k = 0;
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                for (int sx = -1; sx <= 1; sx += 2)
                    v[k++] = center + L * (float)sx + W * (float)sz + H * (float)sy;
        // indices ordered by (sy,sz,sx): 0..3 bottom, 4..7 top
        static const int e[12][2] = {
            {0,1},{2,3},{0,2},{1,3}, {4,5},{6,7},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
        for (auto& pr : e) line(v[pr[0]], v[pr[1]], c);
    }

    // Upload the accumulated lines and draw them, then clear. `vp` = proj * view.
    // Caller controls depth-test/blend state around this call.
    void flush(const glm::mat4& vp)
    {
        if (verts.empty() || !prog) { clear(); return; }
        glUseProgram(prog);
        glUniformMatrix4fv(uVP, 1, GL_FALSE, &vp[0][0]);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)),
                     verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, (GLsizei)(verts.size() / 6));
        glBindVertexArray(0);
        clear();
    }

    void free()
    {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
        prog = vao = vbo = 0;
    }
};

} // namespace smallgine
