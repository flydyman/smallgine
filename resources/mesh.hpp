#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "../platform/glcontext.hpp"

namespace smallgine {

// Indexed GPU geometry: pos(3) + normal(3) + uv(2) + tangent(3), interleaved.
struct Mesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    void draw() const
    {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    void free()
    {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = 0;
        indexCount = 0;
    }
};

// Take interleaved pos(3)+normal(3)+uv(2) verts + indices, compute per-vertex
// tangents, and upload as pos(3)+normal(3)+uv(2)+tangent(3). VAO left unbound.
inline std::shared_ptr<Mesh> uploadMesh(const float* in, size_t vertBytes,
                                        const unsigned int* idx, size_t idxBytes,
                                        GLsizei indexCount)
{
    const int IN = 8, OUT = 11;
    size_t nVerts = vertBytes / (IN * sizeof(float));
    size_t nIdx = idxBytes / sizeof(unsigned int);

    auto P = [&](size_t v, int c) { return in[v * IN + c]; };
    std::vector<float> tan(nVerts * 3, 0.0f);

    for (size_t i = 0; i + 2 < nIdx; i += 3)
    {
        unsigned int a = idx[i], b = idx[i + 1], c = idx[i + 2];
        float e1x = P(b,0)-P(a,0), e1y = P(b,1)-P(a,1), e1z = P(b,2)-P(a,2);
        float e2x = P(c,0)-P(a,0), e2y = P(c,1)-P(a,1), e2z = P(c,2)-P(a,2);
        float du1 = P(b,6)-P(a,6), dv1 = P(b,7)-P(a,7);
        float du2 = P(c,6)-P(a,6), dv2 = P(c,7)-P(a,7);
        float det = du1 * dv2 - du2 * dv1;
        float f = (std::fabs(det) > 1e-8f) ? 1.0f / det : 0.0f;
        float tx = f * (dv2 * e1x - dv1 * e2x);
        float ty = f * (dv2 * e1y - dv1 * e2y);
        float tz = f * (dv2 * e1z - dv1 * e2z);
        for (unsigned int v : {a, b, c}) { tan[v*3]+=tx; tan[v*3+1]+=ty; tan[v*3+2]+=tz; }
    }

    std::vector<float> out(nVerts * OUT);
    for (size_t v = 0; v < nVerts; ++v)
    {
        for (int c = 0; c < IN; ++c) out[v*OUT + c] = in[v*IN + c];
        // Gram-Schmidt orthogonalize tangent against the normal.
        float nx = P(v,3), ny = P(v,4), nz = P(v,5);
        float tx = tan[v*3], ty = tan[v*3+1], tz = tan[v*3+2];
        float d = nx*tx + ny*ty + nz*tz;
        tx -= nx*d; ty -= ny*d; tz -= nz*d;
        float len = std::sqrt(tx*tx + ty*ty + tz*tz);
        if (len < 1e-6f) { tx = 1.0f; ty = 0.0f; tz = 0.0f; len = 1.0f; }
        out[v*OUT+8] = tx/len; out[v*OUT+9] = ty/len; out[v*OUT+10] = tz/len;
    }

    auto m = std::make_shared<Mesh>();
    glGenVertexArrays(1, &m->vao);
    glGenBuffers(1, &m->vbo);
    glGenBuffers(1, &m->ebo);

    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, out.size() * sizeof(float), out.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxBytes, idx, GL_STATIC_DRAW);

    const GLsizei stride = OUT * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);

    m->indexCount = indexCount;
    return m;
}

// Unit quad centered on origin, normal +z, UVs 0..1.
inline std::shared_ptr<Mesh> makeQuad()
{
    static const float verts[] = {
        // x     y     z      nx ny nz   u  v
        -0.5f,-0.5f, 0.0f,   0,0,1,   0,0,
         0.5f,-0.5f, 0.0f,   0,0,1,   1,0,
         0.5f, 0.5f, 0.0f,   0,0,1,   1,1,
        -0.5f, 0.5f, 0.0f,   0,0,1,   0,1,
    };
    static const unsigned int idx[] = { 0, 1, 2, 2, 3, 0 };
    return uploadMesh(verts, sizeof(verts), idx, sizeof(idx), 6);
}

// Unit cube centered on origin (±0.5), per-face normals + UVs. CCW-outward winding.
inline std::shared_ptr<Mesh> makeCube()
{
    static const float verts[] = {
        // front (+z)
        -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,   0.5f,-0.5f, 0.5f,  0,0,1,  1,0,   0.5f, 0.5f, 0.5f,  0,0,1,  1,1,  -0.5f, 0.5f, 0.5f,  0,0,1,  0,1,
        // back (-z)
         0.5f,-0.5f,-0.5f,  0,0,-1, 0,0,  -0.5f,-0.5f,-0.5f,  0,0,-1, 1,0,  -0.5f, 0.5f,-0.5f,  0,0,-1, 1,1,   0.5f, 0.5f,-0.5f,  0,0,-1, 0,1,
        // left (-x)
        -0.5f,-0.5f,-0.5f, -1,0,0,  0,0,  -0.5f,-0.5f, 0.5f, -1,0,0,  1,0,  -0.5f, 0.5f, 0.5f, -1,0,0,  1,1,  -0.5f, 0.5f,-0.5f, -1,0,0,  0,1,
        // right (+x)
         0.5f,-0.5f, 0.5f,  1,0,0,  0,0,   0.5f,-0.5f,-0.5f,  1,0,0,  1,0,   0.5f, 0.5f,-0.5f,  1,0,0,  1,1,   0.5f, 0.5f, 0.5f,  1,0,0,  0,1,
        // top (+y)
        -0.5f, 0.5f, 0.5f,  0,1,0,  0,0,   0.5f, 0.5f, 0.5f,  0,1,0,  1,0,   0.5f, 0.5f,-0.5f,  0,1,0,  1,1,  -0.5f, 0.5f,-0.5f,  0,1,0,  0,1,
        // bottom (-y)
        -0.5f,-0.5f,-0.5f,  0,-1,0, 0,0,   0.5f,-0.5f,-0.5f,  0,-1,0, 1,0,   0.5f,-0.5f, 0.5f,  0,-1,0, 1,1,  -0.5f,-0.5f, 0.5f,  0,-1,0, 0,1,
    };
    unsigned int idx[36];
    for (unsigned int f = 0; f < 6; ++f)
    {
        unsigned int b = f * 4;
        unsigned int o = f * 6;
        idx[o+0] = b+0; idx[o+1] = b+1; idx[o+2] = b+2;
        idx[o+3] = b+2; idx[o+4] = b+3; idx[o+5] = b+0;
    }
    return uploadMesh(verts, sizeof(verts), idx, sizeof(idx), 36);
}

} // namespace smallgine
