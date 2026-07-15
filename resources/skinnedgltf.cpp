#include "skinnedgltf.hpp"
#include "../tools/shader.hpp"
#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace smallgine {

using nlohmann::json;

namespace {
// Raw accessor readers over a set of resolved buffers.
struct GLTFBuf {
    json j;
    std::vector<std::vector<unsigned char>> buffers;

    const unsigned char* base(int acc, int& stride, int comps, int compSize) const
    {
        const auto& a = j["accessors"][acc];
        const auto& bv = j["bufferViews"][a["bufferView"].get<int>()];
        stride = bv.value("byteStride", 0); if (stride == 0) stride = comps * compSize;
        int off = bv.value("byteOffset", 0) + a.value("byteOffset", 0);
        return &buffers[bv.value("buffer", 0)][off];
    }
    int count(int acc) const { return j["accessors"][acc]["count"].get<int>(); }
    int compType(int acc) const { return j["accessors"][acc]["componentType"].get<int>(); }
};
}

bool SkinnedGLTF::load(const std::string& path, const glm::vec3& worldPos)
{
    model = glm::translate(glm::mat4(1.0f), worldPos);
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cout << "SkinnedGLTF load failed: " << path << std::endl; return false; }
    std::vector<unsigned char> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (file.size() < 12) return false;

    GLTFBuf gb;
    std::vector<unsigned char> glbBin;
    bool isGlb = std::memcmp(file.data(), "glTF", 4) == 0;
    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
    if (isGlb)
    {
        size_t p = 12;
        while (p + 8 <= file.size())
        {
            uint32_t len; std::memcpy(&len, &file[p], 4);
            uint32_t type; std::memcpy(&type, &file[p + 4], 4);
            if (type == 0x4E4F534A) gb.j = json::parse(std::string((const char*)&file[p + 8], len), nullptr, false);
            else if (type == 0x004E4942) glbBin.assign(&file[p + 8], &file[p + 8] + len);
            p += 8 + len;
        }
    }
    else { try { gb.j = json::parse(file.begin(), file.end()); } catch (...) { return false; } }
    if (gb.j.is_discarded()) return false;
    json& j = gb.j;

    for (const auto& b : j["buffers"])
    {
        std::string uri = b.value("uri", std::string());
        if (uri.empty()) gb.buffers.push_back(glbBin);
        else { std::ifstream bf(dir + uri, std::ios::binary); gb.buffers.push_back(std::vector<unsigned char>((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>())); }
    }

    // --- Geometry (first skinned primitive): pos3, nrm3, uv2, joint4, weight4 ---
    const auto& prim = j["meshes"][0]["primitives"][0]["attributes"];
    int posA = prim["POSITION"].get<int>();
    int n = gb.count(posA);
    auto readVecF = [&](int acc, int comps) {
        int stride; const unsigned char* p = gb.base(acc, stride, comps, 4);
        std::vector<float> v(n * comps);
        for (int i = 0; i < n; ++i) std::memcpy(&v[i * comps], p + i * stride, comps * 4);
        return v;
    };
    std::vector<float> pos = readVecF(posA, 3);
    std::vector<float> nrm = prim.contains("NORMAL") ? readVecF(prim["NORMAL"].get<int>(), 3) : std::vector<float>(n * 3, 0.0f);
    std::vector<float> uv  = prim.contains("TEXCOORD_0") ? readVecF(prim["TEXCOORD_0"].get<int>(), 2) : std::vector<float>(n * 2, 0.0f);
    std::vector<float> wts = readVecF(prim["WEIGHTS_0"].get<int>(), 4);
    // Joints: ubyte or ushort -> float.
    std::vector<float> jnt(n * 4, 0.0f);
    {
        int jacc = prim["JOINTS_0"].get<int>(), stride, ct = gb.compType(jacc);
        const unsigned char* p = gb.base(jacc, stride, 4, ct == 5123 ? 2 : 1);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 4; ++c)
            {
                unsigned v = (ct == 5123) ? *(const uint16_t*)(p + i * stride + c * 2) : p[i * stride + c];
                jnt[i * 4 + c] = (float)v;
            }
    }
    std::vector<unsigned int> idx;
    {
        int iacc = j["meshes"][0]["primitives"][0]["indices"].get<int>(), stride, ct = gb.compType(iacc);
        int ic = gb.count(iacc);
        const unsigned char* p = gb.base(iacc, stride, 1, ct == 5125 ? 4 : ct == 5123 ? 2 : 1);
        for (int i = 0; i < ic; ++i)
        {
            if (ct == 5125) idx.push_back(*(const uint32_t*)(p + i * 4));
            else if (ct == 5123) idx.push_back(*(const uint16_t*)(p + i * 2));
            else idx.push_back(p[i]);
        }
    }
    indexCount = (GLsizei)idx.size();

    std::vector<float> verts(n * 16);
    for (int i = 0; i < n; ++i)
    {
        float* d = &verts[i * 16];
        d[0]=pos[i*3]; d[1]=pos[i*3+1]; d[2]=pos[i*3+2];
        d[3]=nrm[i*3]; d[4]=nrm[i*3+1]; d[5]=nrm[i*3+2];
        d[6]=uv[i*2]; d[7]=uv[i*2+1];
        d[8]=jnt[i*4]; d[9]=jnt[i*4+1]; d[10]=jnt[i*4+2]; d[11]=jnt[i*4+3];
        d[12]=wts[i*4]; d[13]=wts[i*4+1]; d[14]=wts[i*4+2]; d[15]=wts[i*4+3];
    }

    // --- Skeleton nodes (TRS + children) ---
    nodes.resize(j["nodes"].size());
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& nj = j["nodes"][i];
        GNode& gn = nodes[i];
        if (nj.contains("translation")) gn.bt = glm::vec3(nj["translation"][0], nj["translation"][1], nj["translation"][2]);
        if (nj.contains("scale")) gn.bs = glm::vec3(nj["scale"][0], nj["scale"][1], nj["scale"][2]);
        if (nj.contains("rotation")) gn.br = glm::quat(nj["rotation"][3].get<float>(), nj["rotation"][0].get<float>(), nj["rotation"][1].get<float>(), nj["rotation"][2].get<float>());
        if (nj.contains("children")) for (const auto& c : nj["children"]) gn.children.push_back(c.get<int>());
    }
    if (j.contains("scenes") && j.contains("scene"))
        for (const auto& r : j["scenes"][j["scene"].get<int>()]["nodes"]) sceneRoots.push_back(r.get<int>());

    // --- Skin: joints + inverse bind matrices ---
    const auto& skin = j["skins"][0];
    for (const auto& jt : skin["joints"]) joints.push_back(jt.get<int>());
    {
        int acc = skin["inverseBindMatrices"].get<int>(), stride;
        const unsigned char* p = gb.base(acc, stride, 16, 4);
        invBind.resize(joints.size());
        for (size_t i = 0; i < joints.size(); ++i) std::memcpy(&invBind[i][0][0], p + i * stride, 16 * 4);
    }
    jointMat.assign(joints.size(), glm::mat4(1.0f));

    // --- Animation 0: sample channels for T/R/S ---
    if (j.contains("animations"))
    {
        const auto& anim = j["animations"][0];
        for (const auto& ch : anim["channels"])
        {
            const auto& samp = anim["samplers"][ch["sampler"].get<int>()];
            Chan c;
            c.node = ch["target"]["node"].get<int>();
            std::string path = ch["target"]["path"].get<std::string>();
            c.path = (path == "translation") ? 0 : (path == "rotation") ? 1 : 2;
            int ti = samp["input"].get<int>(), oi = samp["output"].get<int>(), stride;
            int tc = gb.count(ti);
            const unsigned char* tp = gb.base(ti, stride, 1, 4);
            for (int i = 0; i < tc; ++i) c.times.push_back(*(const float*)(tp + i * stride));
            int comps = (c.path == 1) ? 4 : 3, ostride;
            const unsigned char* op = gb.base(oi, ostride, comps, 4);
            for (int i = 0; i < tc; ++i)
            {
                glm::vec4 val(0.0f);
                std::memcpy(&val, op + i * ostride, comps * 4);
                c.values.push_back(val);
            }
            if (!c.times.empty()) animDur = std::max(animDur, c.times.back());
            channels.push_back(c);
        }
    }

    // --- GL upload (16-float skinned vertex) ---
    prog = tools::linkProgramFiles("assets/shaders/skin.vert", "assets/shaders/skin.frag");
    uViewProj = glGetUniformLocation(prog, "uViewProj");
    uModel = glGetUniformLocation(prog, "uModel");
    uJoints = glGetUniformLocation(prog, "uJoints");
    uLightDir = glGetUniformLocation(prog, "uLightDir");
    uColor = glGetUniformLocation(prog, "uColor");
    if (j.contains("materials") && j["materials"][0].contains("pbrMetallicRoughness"))
    {
        const auto& b = j["materials"][0]["pbrMetallicRoughness"];
        if (b.contains("baseColorFactor")) color = glm::vec3(b["baseColorFactor"][0], b["baseColorFactor"][1], b["baseColorFactor"][2]);
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    const GLsizei st = 16 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);                    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void*)(3 * sizeof(float)));  glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void*)(6 * sizeof(float)));  glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)(8 * sizeof(float)));  glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (void*)(12 * sizeof(float))); glEnableVertexAttribArray(4);
    glBindVertexArray(0);

    std::cout << "SkinnedGLTF loaded: " << path << " (" << n << " verts, "
              << joints.size() << " joints, " << channels.size() << " anim channels)" << std::endl;
    return true;
}

void SkinnedGLTF::computeGlobals(int node, const glm::mat4& parent, std::vector<glm::mat4>& out) const
{
    const GNode& gn = nodes[node];
    glm::mat4 local = glm::translate(glm::mat4(1.0f), gn.t) * glm::mat4_cast(gn.r) * glm::scale(glm::mat4(1.0f), gn.s);
    glm::mat4 g = parent * local;
    out[node] = g;
    for (int c : gn.children) computeGlobals(c, g, out);
}

void SkinnedGLTF::update(float time)
{
    if (!ready()) return;
    for (GNode& gn : nodes) { gn.t = gn.bt; gn.r = gn.br; gn.s = gn.bs; } // reset to bind pose

    float t = animDur > 0.0f ? std::fmod(time, animDur) : 0.0f;
    for (const Chan& c : channels)
    {
        if (c.times.empty()) continue;
        size_t i = 0;
        while (i + 1 < c.times.size() && c.times[i + 1] < t) ++i;
        size_t j = std::min(i + 1, c.times.size() - 1);
        float span = std::max(c.times[j] - c.times[i], 1e-6f);
        float f = (j > i) ? (t - c.times[i]) / span : 0.0f;
        GNode& gn = nodes[c.node];
        if (c.path == 0) gn.t = glm::mix(glm::vec3(c.values[i]), glm::vec3(c.values[j]), f);
        else if (c.path == 2) gn.s = glm::mix(glm::vec3(c.values[i]), glm::vec3(c.values[j]), f);
        else {
            glm::quat qa(c.values[i].w, c.values[i].x, c.values[i].y, c.values[i].z);
            glm::quat qb(c.values[j].w, c.values[j].x, c.values[j].y, c.values[j].z);
            gn.r = glm::slerp(qa, qb, f);
        }
    }

    std::vector<glm::mat4> globals(nodes.size(), glm::mat4(1.0f));
    for (int r : sceneRoots) computeGlobals(r, glm::mat4(1.0f), globals);
    for (size_t k = 0; k < joints.size(); ++k) jointMat[k] = globals[joints[k]] * invBind[k];
}

void SkinnedGLTF::draw(const glm::mat4& viewProj, const glm::vec3& lightDir)
{
    if (!ready()) return;
    glUseProgram(prog);
    glUniformMatrix4fv(uViewProj, 1, GL_FALSE, &viewProj[0][0]);
    glUniformMatrix4fv(uModel, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(uJoints, (GLsizei)jointMat.size(), GL_FALSE, &jointMat[0][0][0]);
    glm::vec3 ld = glm::normalize(lightDir);
    glUniform3fv(uLightDir, 1, &ld[0]);
    glUniform3fv(uColor, 1, &color[0]);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
}

void SkinnedGLTF::free()
{
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
}

} // namespace smallgine
