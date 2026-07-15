#pragma once
#include <nlohmann/json.hpp>
#include "../platform/glcontext.hpp"
#include "../platform/paths.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "scene.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "light.hpp"
#include "../tools/helpers.hpp"
#include "../tools/shader.hpp"
#include "../tools/glcheck.hpp"
#include "../tools/text.hpp"
#include "../resources/mesh.hpp"
#include "../resources/objloader.hpp"
#include "../resources/texture.hpp"
#include "../resources/skybox.hpp"
#include "../resources/shadow.hpp"
#include "../resources/instanced.hpp"
#include "../resources/postfx.hpp"
#include "../audio/audio.hpp"

namespace smallgine {

namespace {
    const int kMaxLights = 8;

    const char* kVertSrc =
        "#version 310 es\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec2 aUV;\n"
        "layout(location = 3) in vec3 aTangent;\n"
        "uniform mat4 uMVP;\n"
        "uniform mat4 uModel;\n"
        "uniform mat4 uLightSpace;\n"
        "out vec2 vUV;\n"
        "out vec3 vNormal;\n"
        "out vec3 vTangent;\n"
        "out vec3 vWorldPos;\n"
        "out vec4 vLightSpacePos;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    mat3 nm = mat3(transpose(inverse(uModel)));\n" // correct for non-uniform scale
        "    vNormal = nm * aNormal;\n"
        "    vTangent = nm * aTangent;\n"
        "    vec4 wp = uModel * vec4(aPos, 1.0);\n"
        "    vWorldPos = wp.xyz;\n"
        "    vLightSpacePos = uLightSpace * wp;\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* kFragSrc =
        "#version 310 es\n"
        "precision mediump float;\n"
        "const int MAXL = 8;\n"
        "in vec2 vUV;\n"
        "in vec3 vNormal;\n"
        "in vec3 vTangent;\n"
        "in vec3 vWorldPos;\n"
        "in vec4 vLightSpacePos;\n"
        "uniform sampler2D uTex;\n"
        "uniform sampler2D uShadowMap;\n"
        "uniform sampler2D uNormalMap;\n"
        "uniform bool uHasNormalMap;\n"
        "uniform float uAlpha;\n"
        "uniform vec3 uColor;\n"
        "uniform float uShininess;\n"
        "uniform float uSpecular;\n"
        "uniform vec3 uViewPos;\n"
        "uniform int uNumLights;\n"
        "uniform int uLightType[MAXL];\n"
        "uniform vec3 uLightPos[MAXL];\n"
        "uniform vec3 uLightColor[MAXL];\n"
        "uniform float uLightIntensity[MAXL];\n"
        "out vec4 FragColor;\n"
        "float shadowFactor(vec3 n, vec3 L) {\n"
        "    vec3 p = vLightSpacePos.xyz / vLightSpacePos.w;\n"
        "    p = p * 0.5 + 0.5;\n"
        "    if (p.z > 1.0) return 0.0;\n"
        "    float bias = max(0.0025 * (1.0 - dot(n, L)), 0.0008);\n"
        "    vec2 texel = vec2(1.0 / 1024.0);\n"
        "    float sh = 0.0;\n"
        "    for (int x = -1; x <= 1; x++) {\n"
        "        for (int y = -1; y <= 1; y++) {\n"
        "            float closest = texture(uShadowMap, p.xy + vec2(float(x), float(y)) * texel).r;\n"
        "            sh += (p.z - bias > closest) ? 1.0 : 0.0;\n"
        "        }\n"
        "    }\n"
        "    return sh / 9.0;\n" // 3x3 PCF => soft edges
        "}\n"
        "void main() {\n"
        "    vec3 n = normalize(vNormal);\n"
        "    if (uHasNormalMap) {\n"
        "        vec3 T = normalize(vTangent - n * dot(n, vTangent));\n"
        "        vec3 B = cross(n, T);\n"
        "        vec3 nm = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;\n"
        "        n = normalize(mat3(T, B, n) * nm);\n"
        "    }\n"
        "    vec3 viewDir = normalize(uViewPos - vWorldPos);\n"
        "    vec4 tex = texture(uTex, vUV);\n"
        "    vec3 base = tex.rgb * uColor;\n"
        "    vec3 result = 0.15 * base;\n" // ambient
        "    for (int i = 0; i < uNumLights; i++) {\n"
        "        vec3 L = (uLightType[i] == 0)\n"
        "            ? normalize(uLightPos[i])\n"
        "            : normalize(uLightPos[i] - vWorldPos);\n"
        "        float diff = max(dot(n, L), 0.0);\n"
        "        vec3 h = normalize(L + viewDir);\n"
        "        float spec = pow(max(dot(n, h), 0.0), uShininess) * uSpecular;\n"
        "        vec3 lc = uLightColor[i] * uLightIntensity[i];\n"
        "        float sh = (uLightType[i] == 0) ? shadowFactor(n, L) : 0.0;\n"
        "        result += lc * (1.0 - sh) * (diff * base + spec);\n"
        "    }\n"
        "    FragColor = vec4(result, tex.a * uAlpha);\n"
        "}\n";

    // Demo scene as JSON. Materials per node; leftChild (octa) inherits its .mtl.
    const char* kSceneJson = R"({
        "name": "demo",
        "description": "iter19",
        "lights": [
            { "type": 0, "position": [0.4, 1.0, 0.6], "color": [1.0, 1.0, 1.0], "intensity": 0.7 },
            { "type": 1, "position": [1.5, 1.5, 2.5], "color": [1.0, 0.5, 0.2], "intensity": 2.0 }
        ],
        "root": {
            "name": "root",
            "position": [0.0, 0.0, 0.0],
            "children": [
                {
                    "name": "ground",
                    "position": [0.0, -1.6, 0.0],
                    "scale": [6.0, 0.2, 6.0],
                    "material": { "color": [0.7, 0.7, 0.75], "texture": "", "normalMap": "assets/normal.tga", "shininess": 24.0, "specular": 0.4 }
                },
                {
                    "name": "content",
                    "position": [0.0, 0.3, 0.0],
                    "spin": [10.0, 30.0, 0.0],
                    "children": [
                        {
                            "name": "left",
                            "position": [-1.1, 0.0, 0.3],
                            "scale": [0.6, 0.6, 0.6],
                            "material": { "color": [1.0, 0.7, 0.7], "texture": "assets/test2.tga", "shininess": 64.0, "specular": 0.8 },
                            "children": [
                                { "name": "leftChild", "position": [0.0, 1.6, 0.0], "scale": [0.8, 0.8, 0.8], "spin": [0.0, 0.0, 120.0], "mesh": "assets/octa.obj" }
                            ]
                        },
                        {
                            "name": "right",
                            "position": [1.1, 0.0, -0.6],
                            "rotation": [20.0, 30.0, 0.0],
                            "scale": [0.6, 0.6, 0.6],
                            "material": { "color": [0.7, 1.0, 0.8], "texture": "assets/test2.tga", "shininess": 16.0, "specular": 0.3 }
                        },
                        {
                            "name": "glass",
                            "position": [0.0, 0.4, 1.6],
                            "scale": [2.2, 1.6, 0.06],
                            "material": { "color": [0.5, 0.8, 1.0], "texture": "", "shininess": 96.0, "specular": 1.0, "alpha": 0.35 }
                        }
                    ]
                }
            ]
        }
    })";
}

struct LoadedModel
{
    std::shared_ptr<Mesh> mesh;
    Material material;
    bool hasMaterial = false;
};

class Engine {
private:
    bool isInitialized;
    bool isLooped;

    GLuint program = 0;
    GLint  uMVP = -1, uModel = -1, uTex = -1, uColor = -1;
    GLint  uShininess = -1, uSpecular = -1, uViewPos = -1;
    GLint  uNumLights = -1, uLightType = -1, uLightPos = -1, uLightColor = -1, uLightIntensity = -1;
    GLint  uLightSpace = -1, uShadowMap = -1;
    GLint  uNormalMap = -1, uHasNormalMap = -1, uAlpha = -1;

    std::shared_ptr<Mesh> cube;
    std::map<std::string, LoadedModel> models;   // OBJ path -> mesh + material (cache)
    std::map<std::string, Texture> textures;     // path -> loaded GL texture (cache)
    std::string texturePath = "assets/test.tga"; // engine default texture
    Scene scene;
    Camera camera;
    AudioSystem audio;
    Skybox skybox;
    ShadowMap shadow;
    TextRenderer text;
    InstancedField instances;
    GLuint instanceTex = 0;
    PostFX post;
    int postW = 0, postH = 0;
    int currentFps = 0;
    std::vector<Light> lights;

    // Held movement keys (WASD move in plane, QE up/down).
    bool keyW = false, keyS = false, keyA = false, keyD = false, keyQ = false, keyE = false;

    // Mouse-look state.
    double lastX = 0.0, lastY = 0.0;
    bool firstMouse = true;
    bool mouseCaptured = true;
    GLFWwindow* window = nullptr;

    // FPS reporting.
    double fpsAccum = 0.0;
    int fpsFrames = 0;

    int spawnCount = 0;

    // Runtime spawn: add a small cube node under "content".
    void spawnNode()
    {
        Node* content = scene.MainNode.find("content");
        if (!content) return;
        Node n;
        n.Name = "spawn" + std::to_string(spawnCount);
        float fx = (float)(std::rand() % 200 - 100) / 60.0f;
        float fz = (float)(std::rand() % 200 - 100) / 60.0f;
        n.Position = glm::vec3(fx, 1.2f, fz);
        n.Scale = glm::vec3(0.3f);
        n.Spin = glm::vec3(0.0f, 120.0f, 40.0f);
        n.material.color = glm::vec3((std::rand()%100)/100.0f, (std::rand()%100)/100.0f, (std::rand()%100)/100.0f);
        n.material.texture = "assets/test.tga";
        n.materialSet = true;
        content->addChild(n);
        prepareNodes(*content); // re-resolve mesh/textures (cached)
        spawnCount++;
        std::cout << "Spawned " << n.Name << " (nodes now +" << spawnCount << ")" << std::endl;
    }

    void despawnNode()
    {
        Node* content = scene.MainNode.find("content");
        if (!content || spawnCount == 0) return;
        std::string name = "spawn" + std::to_string(spawnCount - 1);
        if (content->removeChild(name))
        {
            spawnCount--;
            std::cout << "Despawned " << name << std::endl;
        }
    }

    // Serialize the live scene to JSON on disk.
    void saveScene()
    {
        nlohmann::json j = scene;
        std::string path = resolvePath("scene_saved.json");
        std::ofstream f(path);
        if (!f) { std::cout << "Scene save failed: " << path << std::endl; return; }
        f << j.dump(2);
        std::cout << "Scene saved: " << path << std::endl;
    }

    // Hot-reload the scene from the saved JSON file.
    void reloadScene()
    {
        std::string path = resolvePath("scene_saved.json");
        std::ifstream f(path);
        if (!f) { std::cout << "No saved scene at " << path << std::endl; return; }
        nlohmann::json j;
        f >> j;
        scene = j.get<Scene>();
        prepareNodes(scene.MainNode);
        if (!scene.Lights.empty()) lights = scene.Lights;
        spawnCount = 0;
        std::cout << "Scene reloaded: '" << scene.name << "'" << std::endl;
    }

    // Apply each node's angular velocity (behavior hook), recursively.
    void applySpin(Node& n, float dt)
    {
        n.Rotation += n.Spin * dt;
        for (Node& child : n.Children)
        {
            applySpin(child, dt);
        }
    }

    // Load (or fetch cached) texture for a path; empty path => engine default.
    GLuint textureFor(const std::string& path)
    {
        std::string key = path.empty() ? texturePath : path;
        auto it = textures.find(key);
        if (it != textures.end()) return it->second.id;

        Texture t = loadTexture(resolvePath(key).c_str());
        if (t.id == 0)
        {
            std::cout << "Texture fallback (checker) for: " << key << std::endl;
            t = makeCheckerTexture(8);
        }
        textures[key] = t;
        return t.id;
    }

    // Load (or fetch cached) OBJ mesh + its .mtl material; cube fallback on failure.
    LoadedModel& modelFor(const std::string& path)
    {
        auto it = models.find(path);
        if (it != models.end()) return it->second;

        LoadedModel lm;
        Material m;
        bool has = false;
        lm.mesh = loadOBJ(resolvePath(path), &m, &has);
        if (!lm.mesh)
        {
            std::cout << "Mesh fallback (cube) for: " << path << std::endl;
            lm.mesh = cube;
        }
        lm.material = m;
        lm.hasMaterial = has;
        models[path] = lm;
        return models[path];
    }

    // Resolve geometry, inherit OBJ material where the node didn't override, load texture.
    void prepareNodes(Node& n)
    {
        if (!n.MeshPath.empty())
        {
            LoadedModel& lm = modelFor(n.MeshPath);
            n.mesh = lm.mesh;
            // Node's own material block wins; otherwise inherit the OBJ's .mtl.
            if (lm.hasMaterial && !n.materialSet)
            {
                n.material = lm.material;
            }
        }
        else
        {
            n.mesh = cube;
        }
        n.texId = textureFor(n.material.texture);
        n.normalTexId = n.material.normalMap.empty() ? 0 : textureFor(n.material.normalMap);
        for (Node& child : n.Children)
        {
            prepareNodes(child);
        }
    }

public:
    void setWindow(GLFWwindow* w) { window = w; }

    void configure(const Config& cfg)
    {
        camera.speed = cfg.cameraSpeed;
        camera.sensitivity = cfg.mouseSensitivity;
        texturePath = cfg.texture;
    }

    Engine()
    {
        isInitialized = true;
        isLooped = false;
        std::cout << "Engine created" << std::endl;
    }

    bool looped() { return isLooped; }

    void create()
    {
        isLooped = true;

        // Frame the scene so the ground + shadows are visible.
        camera.position = glm::vec3(0.0f, 1.8f, 5.0f);
        camera.pitch = -18.0f;
        camera.updateVectors();

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        program = tools::linkProgram(kVertSrc, kFragSrc);
        if (program == 0)
        {
            std::cout << "Fatal: shader program failed to link" << std::endl;
            isLooped = false;
            return;
        }
        uMVP = glGetUniformLocation(program, "uMVP");
        uModel = glGetUniformLocation(program, "uModel");
        uTex = glGetUniformLocation(program, "uTex");
        uColor = glGetUniformLocation(program, "uColor");
        uShininess = glGetUniformLocation(program, "uShininess");
        uSpecular = glGetUniformLocation(program, "uSpecular");
        uViewPos = glGetUniformLocation(program, "uViewPos");
        uNumLights = glGetUniformLocation(program, "uNumLights");
        uLightType = glGetUniformLocation(program, "uLightType");
        uLightPos = glGetUniformLocation(program, "uLightPos");
        uLightColor = glGetUniformLocation(program, "uLightColor");
        uLightIntensity = glGetUniformLocation(program, "uLightIntensity");
        uLightSpace = glGetUniformLocation(program, "uLightSpace");
        uShadowMap = glGetUniformLocation(program, "uShadowMap");
        uNormalMap = glGetUniformLocation(program, "uNormalMap");
        uHasNormalMap = glGetUniformLocation(program, "uHasNormalMap");
        uAlpha = glGetUniformLocation(program, "uAlpha");

        cube = makeCube();
        skybox.init();
        shadow.init(1024);
        text.init(resolvePath("assets/font.ttf"), 22.0f);

        scene = loadSceneFromString(kSceneJson);
        prepareNodes(scene.MainNode);
        tools::glCheck("resource setup");

        // Instanced ring of small cubes around the scene (one draw call).
        {
            std::vector<glm::mat4> mats;
            const int N = 48;
            for (int i = 0; i < N; ++i)
            {
                float a = (float)i / N * 6.28318530718f;
                glm::mat4 m(1.0f);
                m = glm::translate(m, glm::vec3(std::cos(a) * 3.6f, -1.35f, std::sin(a) * 3.6f));
                m = glm::rotate(m, a, glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(0.22f));
                mats.push_back(m);
            }
            instances.init(cube, mats);
            instanceTex = textureFor("assets/test.tga");
        }

        // Lights come from the scene; fall back to sensible defaults if none.
        lights = scene.Lights;
        if (lights.empty())
        {
            lights.push_back({0, glm::normalize(glm::vec3(0.4f, 1.0f, 0.6f)), glm::vec3(1.0f), 0.7f});
            lights.push_back({1, glm::vec3(1.5f, 1.5f, 2.5f), glm::vec3(1.0f, 0.5f, 0.2f), 2.0f});
        }

        if (audio.init())
        {
            audio.playTone(440.0f, 0.2f);
        }

        std::cout << "Engine initialized: scene '" << scene.name << "', "
                  << lights.size() << " lights" << std::endl;
    }

    void uploadLights()
    {
        int n = (int)lights.size();
        if (n > kMaxLights) n = kMaxLights;

        int types[kMaxLights];
        float pos[kMaxLights * 3], col[kMaxLights * 3], inten[kMaxLights];
        for (int i = 0; i < n; ++i)
        {
            types[i] = lights[i].type;
            pos[i*3+0] = lights[i].position.x; pos[i*3+1] = lights[i].position.y; pos[i*3+2] = lights[i].position.z;
            col[i*3+0] = lights[i].color.x;    col[i*3+1] = lights[i].color.y;    col[i*3+2] = lights[i].color.z;
            inten[i] = lights[i].intensity;
        }
        glUniform1i(uNumLights, n);
        glUniform1iv(uLightType, n, types);
        glUniform3fv(uLightPos, n, pos);
        glUniform3fv(uLightColor, n, col);
        glUniform1fv(uLightIntensity, n, inten);
    }

    void draw(GLFWwindow * window)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = height > 0 ? (float)width / (float)height : 1.0f;

        glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect, 0.1f, 100.0f);
        glm::mat4 viewProj = proj * camera.view();

        // Directional light drives the shadow map.
        glm::vec3 dirToLight(0.4f, 1.0f, 0.6f);
        for (const Light& l : lights) { if (l.type == 0) { dirToLight = l.position; break; } }
        glm::mat4 lightSpace = shadow.lightSpace(dirToLight);

        // Pass 1: scene depth from the light's view.
        shadow.begin();
        tools::DrawSceneDepth(scene, shadow.uLightMVP, lightSpace);
        shadow.end(width, height);

        // Offscreen target for post-processing (lazy init + resize).
        if (postW != width || postH != height)
        {
            if (postW == 0) post.init(width, height);
            else post.resize(width, height);
            postW = width; postH = height;
        }
        post.bind();

        // Pass 2: lit scene, sampling the shadow map.
        glClearColor(0.5f, 0.2f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        glUniform1i(uTex, 0);
        glUniform1i(uShadowMap, 1);
        glUniform1i(uNormalMap, 2);
        glUniformMatrix4fv(uLightSpace, 1, GL_FALSE, &lightSpace[0][0]);
        glUniform3fv(uViewPos, 1, &camera.position[0]);
        uploadLights();

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadow.texture());
        glActiveTexture(GL_TEXTURE0); // node textures bind to unit 0

        tools::DrawUniforms u;
        u.mvp = uMVP;
        u.model = uModel;
        u.color = uColor;
        u.shininess = uShininess;
        u.specular = uSpecular;
        u.alpha = uAlpha;
        u.hasNormalMap = uHasNormalMap;

        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);

        // Opaque pass.
        for (const tools::DrawItem& it : items)
        {
            if (it.node->material.alpha >= 1.0f) tools::drawItem(it, u, viewProj);
        }

        instances.draw(viewProj, instanceTex, glm::normalize(dirToLight)); // one call, 48 cubes

        skybox.draw(camera.view(), proj); // fills the background (depth == far)

        // Transparent pass: back-to-front, blended, no depth write.
        std::vector<const tools::DrawItem*> trans;
        for (const tools::DrawItem& it : items)
        {
            if (it.node->material.alpha < 1.0f) trans.push_back(&it);
        }
        if (!trans.empty())
        {
            std::sort(trans.begin(), trans.end(), [&](const tools::DrawItem* a, const tools::DrawItem* b) {
                float da = glm::length(camera.position - glm::vec3(a->global[3]));
                float db = glm::length(camera.position - glm::vec3(b->global[3]));
                return da > db;
            });
            glUseProgram(program);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, shadow.texture());
            glActiveTexture(GL_TEXTURE0);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (const tools::DrawItem* it : trans) tools::drawItem(*it, u, viewProj);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }

        // Resolve offscreen scene to the screen with post-processing.
        post.draw(width, height);

        // HUD overlay: 2D text, no depth, no cull, alpha-blended.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        text.draw("smallgine  FPS " + std::to_string(currentFps), 12.0f, 26.0f,
                  width, height, glm::vec3(1.0f, 1.0f, 1.0f));
        text.draw("WASD move | mouse | scroll zoom | TAB cursor | N/M spawn | F5 save | F9 load | ESC quit",
                  12.0f, (float)height - 14.0f, width, height, glm::vec3(0.9f, 0.9f, 0.6f));
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        tools::glCheck("draw");
    }

    void process(double dt)
    {
        float v = camera.speed * (float)dt;
        glm::vec3 right = camera.right();
        if (keyW) camera.position += camera.front * v;
        if (keyS) camera.position -= camera.front * v;
        if (keyA) camera.position -= right * v;
        if (keyD) camera.position += right * v;
        if (keyQ) camera.position += camera.up * v;
        if (keyE) camera.position -= camera.up * v;

        // Per-node behavior: data-driven spin from the scene graph.
        applySpin(scene.MainNode, (float)dt);

        // FPS report once per second.
        fpsAccum += dt;
        fpsFrames++;
        if (fpsAccum >= 1.0)
        {
            currentFps = fpsFrames;
            fpsAccum = 0.0;
            fpsFrames = 0;
        }
    }

    void input(int key, int /*scancode*/, int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            isLooped = false;
            return;
        }

        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        {
            audio.playTone(660.0f, 0.15f);
            return;
        }

        if (action == GLFW_PRESS)
        {
            switch (key)
            {
                case GLFW_KEY_N:  spawnNode();   return;
                case GLFW_KEY_M:  despawnNode(); return;
                case GLFW_KEY_F5: saveScene();   return;
                case GLFW_KEY_F9: reloadScene(); return;
                default: break;
            }
        }

        if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        {
            mouseCaptured = !mouseCaptured;
            if (window)
            {
                glfwSetInputMode(window, GLFW_CURSOR,
                                 mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            }
            firstMouse = true;
            std::cout << "Mouse capture: " << (mouseCaptured ? "on" : "off") << std::endl;
            return;
        }

        bool held = (action != GLFW_RELEASE);
        switch (key)
        {
            case GLFW_KEY_W: keyW = held; break;
            case GLFW_KEY_S: keyS = held; break;
            case GLFW_KEY_A: keyA = held; break;
            case GLFW_KEY_D: keyD = held; break;
            case GLFW_KEY_Q: keyQ = held; break;
            case GLFW_KEY_E: keyE = held; break;
            default: break;
        }
    }

    void scroll(double yoffset)
    {
        camera.addZoom((float)yoffset);
    }

    void mouse(double xpos, double ypos)
    {
        if (!mouseCaptured) return;

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float dx = (float)(xpos - lastX);
        float dy = (float)(lastY - ypos);
        lastX = xpos;
        lastY = ypos;

        camera.addLook(dx, dy);
    }

    void cleanup()
    {
        audio.cleanup();
        skybox.free();
        shadow.free();
        text.free();
        instances.free();
        post.free();
        if (cube) cube->free();
        for (auto& kv : models)
        {
            if (kv.second.mesh) kv.second.mesh->free(); // alias-safe (free zeroes handles)
        }
        for (auto& kv : textures)
        {
            if (kv.second.id) glDeleteTextures(1, &kv.second.id);
        }
        if (program) glDeleteProgram(program);
        std::cout << "Engine destroyed" << std::endl;
    }
};

extern Engine engine;

} // namespace smallgine
