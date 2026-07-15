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
#include "constants.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "light.hpp"
#include "../tools/helpers.hpp"
#include "../tools/shader.hpp"
#include "../tools/glcheck.hpp"
#include "../tools/text.hpp"
#include "../tools/frustum.hpp"
#include "../resources/mesh.hpp"
#include "../resources/objloader.hpp"
#include "../resources/gltf.hpp"
#include "../resources/texture.hpp"
#include "../resources/skybox.hpp"
#include "../resources/shadow.hpp"
#include "../resources/instanced.hpp"
#include "../resources/postfx.hpp"
#include "../audio/audio.hpp"

namespace smallgine {

namespace {



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
                    "material": { "color": [0.7, 0.7, 0.75], "texture": "", "normalMap": "assets/normal.tga", "heightMap": "assets/height.tga", "parallax": 0.04, "shininess": 24.0, "specular": 0.4 }
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
                        },
                        {
                            "name": "gltfNode",
                            "position": [0.0, 1.4, 0.0],
                            "scale": [0.7, 0.7, 0.7],
                            "spin": [0.0, 90.0, 0.0],
                            "mesh": "assets/tetra.gltf",
                            "material": { "color": [0.6, 1.0, 0.9], "texture": "assets/test2.tga", "shininess": 48.0, "specular": 0.6 }
                        },
                        {
                            "name": "lodDemo",
                            "position": [0.0, 2.6, 0.0],
                            "scale": [0.6, 0.6, 0.6],
                            "spin": [0.0, 60.0, 0.0],
                            "material": { "color": [1.0, 0.6, 0.9], "texture": "assets/test.tga", "shininess": 32.0, "specular": 0.5 },
                            "lod": [
                                { "maxDistance": 6.0, "mesh": "assets/tetra.gltf" },
                                { "maxDistance": 11.0, "mesh": "assets/octa.obj" },
                                { "maxDistance": 1000.0, "mesh": "" }
                            ]
                        },
                        {
                            "name": "bob",
                            "scale": [0.35, 0.35, 0.35],
                            "material": { "color": [1.0, 0.9, 0.3], "texture": "assets/test.tga", "shininess": 48.0, "specular": 0.7 },
                            "animation": [
                                { "t": 0.0, "position": [0.0, 0.0, -1.6], "scale": [0.35, 0.35, 0.35] },
                                { "t": 1.5, "position": [0.0, 1.7, -1.6], "scale": [0.55, 0.55, 0.55] },
                                { "t": 3.0, "position": [0.0, 0.0, -1.6], "scale": [0.35, 0.35, 0.35] }
                            ]
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
    GLint  uNormalMap = -1, uHasNormalMap = -1, uAlpha = -1, uSelected = -1;
    GLint  uHeightMap = -1, uParallax = -1;

    std::shared_ptr<Mesh> cube;
    std::map<std::string, LoadedModel> models;   // OBJ path -> mesh + material (cache)
    std::map<std::string, Texture> textures;     // path -> loaded GL texture (cache)
    std::string texturePath = "assets/test.tga"; // engine default texture
    std::string scenePath;                        // scene JSON file (config); empty => embedded
    Scene scene;
    Camera camera;
    std::vector<Camera> camPresets;
    int activeCam = 0;
    const Node* selectedNode = nullptr;
    std::string selectedName;
    AudioSystem audio;
    Skybox skybox;
    ShadowMap shadow;
    TextRenderer text;
    InstancedField instances;
    GLuint instanceTex = 0;
    PostFX post;
    int postW = 0, postH = 0;
    glm::mat4 prevViewProj{1.0f};
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

    // Ray-cast down the camera's forward axis; select the nearest node hit.
    void pick()
    {
        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        glm::vec3 o = camera.position, d = camera.front;
        const Node* best = nullptr;
        float bestT = 1e30f;
        for (const tools::DrawItem& it : items)
        {
            glm::vec3 c(it.global[3]);
            float r = tools::worldRadius(it.global);
            glm::vec3 oc = o - c;
            float b = glm::dot(oc, d);
            float cc = glm::dot(oc, oc) - r * r;
            float disc = b * b - cc;
            if (disc < 0.0f) continue;
            float t = -b - std::sqrt(disc);
            if (t > 0.0f && t < bestT) { bestT = t; best = it.node; }
        }
        selectedNode = best;
        selectedName = best ? best->Name : std::string();
        std::cout << "Picked: " << (best ? best->Name : std::string("none")) << std::endl;
    }

    // Editor gizmo: translate the selected node in its local space.
    void moveSelected(const glm::vec3& delta)
    {
        if (selectedName.empty()) return;
        if (Node* n = scene.MainNode.find(selectedName)) n->Position += delta;
    }

    void cycleCamera()
    {
        if (camPresets.empty()) return;
        activeCam = (activeCam + 1) % (int)camPresets.size();
        camera = camPresets[activeCam];
        std::cout << "Camera preset " << activeCam << std::endl;
    }

    // Serialize the live scene to JSON on disk.
    void saveScene()
    {
        scene.camera = camera;
        scene.hasCamera = true;
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
        if (scene.hasCamera) camera = scene.camera;
        spawnCount = 0;
        std::cout << "Scene reloaded: '" << scene.name << "'" << std::endl;
    }

    double animTime = 0.0;
    int lastVisible = 0, lastTotal = 0, lastOccluded = 0;

    // Occlusion culling (1-frame-deferred bounding-box queries).
    GLuint occProg = 0;
    GLint occMVP = -1;
    std::map<const Node*, GLuint> occQuery;
    std::map<const Node*, char> occVisible;

    // Pick each LOD node's mesh by distance to the camera.
    void updateLOD(Node& n, const glm::mat4& parentModel)
    {
        glm::mat4 global = parentModel * n.localMatrix();
        if (!n.Lod.empty() && !n.lodMeshes.empty())
        {
            float dist = glm::length(camera.position - glm::vec3(global[3]));
            size_t pick = n.lodMeshes.size() - 1;
            for (size_t i = 0; i < n.Lod.size(); ++i)
            {
                if (dist <= n.Lod[i].maxDistance) { pick = i; break; }
            }
            n.mesh = n.lodMeshes[pick];
        }
        for (Node& c : n.Children) updateLOD(c, global);
    }

    // Per-node update: keyframe animation (if any) then spin, recursively.
    void updateNode(Node& n, float dt, float time)
    {
        n.evalAnimation(time);
        n.Rotation += n.Spin * dt;
        for (Node& child : n.Children)
        {
            updateNode(child, dt, time);
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
        bool isGltf = path.size() > 5 && path.substr(path.size() - 5) == ".gltf";
        lm.mesh = isGltf ? loadGLTF(resolvePath(path)) : loadOBJ(resolvePath(path), &m, &has);
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
        n.lodMeshes.clear();
        for (const LodLevel& lv : n.Lod)
        {
            n.lodMeshes.push_back(lv.mesh.empty() ? cube : modelFor(lv.mesh).mesh);
        }
        n.texId = textureFor(n.material.texture);
        n.normalTexId = n.material.normalMap.empty() ? 0 : textureFor(n.material.normalMap);
        n.heightTexId = n.material.heightMap.empty() ? 0 : textureFor(n.material.heightMap);
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
        scenePath = cfg.scene;
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

        // Camera presets (cycled with 'C').
        camPresets.push_back(camera); // front
        Camera top; top.position = {0.0f, 8.0f, 0.2f}; top.pitch = -88.0f; top.updateVectors(); camPresets.push_back(top);
        Camera side; side.position = {6.0f, 1.5f, 0.0f}; side.yaw = 180.0f; side.pitch = -8.0f; side.updateVectors(); camPresets.push_back(side);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        program = tools::linkProgramFiles("assets/shaders/lit.vert", "assets/shaders/lit.frag");
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
        uSelected = glGetUniformLocation(program, "uSelected");
        uHeightMap = glGetUniformLocation(program, "uHeightMap");
        uParallax = glGetUniformLocation(program, "uParallax");

        cube = makeCube();
        occProg = tools::linkProgramFiles("assets/shaders/occ.vert", "assets/shaders/empty.frag");
        occMVP = glGetUniformLocation(occProg, "uMVP");
        skybox.init();
        shadow.init(k::ShadowMapSize);
        text.init(resolvePath("assets/font.ttf"), 22.0f);

        // Scene from config file if set, else the built-in demo.
        bool loaded = false;
        if (!scenePath.empty())
        {
            std::ifstream f(resolvePath(scenePath));
            if (f)
            {
                nlohmann::json j; f >> j;
                scene = j.get<Scene>();
                loaded = true;
                std::cout << "Scene from file: " << scenePath << std::endl;
            }
            else std::cout << "Scene file missing (" << scenePath << "), using built-in" << std::endl;
        }
        if (!loaded) scene = loadSceneFromString(kSceneJson);
        prepareNodes(scene.MainNode);
        tools::glCheck("resource setup");

        // Instanced ring of small cubes around the scene (one draw call).
        {
            std::vector<glm::mat4> mats;
            const int N = k::InstanceRingCount;
            for (int i = 0; i < N; ++i)
            {
                float a = (float)i / N * 6.28318530718f;
                glm::mat4 m(1.0f);
                m = glm::translate(m, glm::vec3(std::cos(a) * k::InstanceRingRadius, k::InstanceRingY, std::sin(a) * k::InstanceRingRadius));
                m = glm::rotate(m, a, glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(k::InstanceScale));
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
            audio.loadEmitter(resolvePath("assets/blip.wav").c_str(), glm::vec3(-1.1f, 0.3f, 0.3f));
        }

        std::cout << "Engine initialized: scene '" << scene.name << "', "
                  << lights.size() << " lights" << std::endl;
    }

    void uploadLights()
    {
        int n = (int)lights.size();
        if (n > k::MaxLights) n = k::MaxLights;

        int types[k::MaxLights];
        float pos[k::MaxLights * 3], col[k::MaxLights * 3], inten[k::MaxLights];
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

        glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect, k::PerspectiveNear, k::PerspectiveFar);
        glm::mat4 viewProj = proj * camera.view();

        // Directional light drives the shadow map.
        glm::vec3 dirToLight(0.4f, 1.0f, 0.6f);
        for (const Light& l : lights) { if (l.type == 0) { dirToLight = l.position; break; } }
        glm::mat4 lightSpace = shadow.lightSpace(dirToLight);

        // Pass 1: scene depth from the light's view.
        shadow.begin();
        tools::DrawSceneDepth(scene, shadow.uLightMVP, lightSpace);
        instances.drawDepth(lightSpace); // instanced ring casts shadows too
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
        glUniform1i(uHeightMap, 3);
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
        u.parallax = uParallax;

        updateLOD(scene.MainNode, glm::mat4(1.0f)); // distance-based mesh swap

        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);

        // Frustum culling.
        tools::Frustum frustum;
        frustum.fromMatrix(viewProj);
        lastTotal = (int)items.size();
        lastVisible = 0;
        lastOccluded = 0;

        // Read last frame's occlusion-query results.
        for (auto& kv : occQuery)
        {
            GLuint avail = 0;
            glGetQueryObjectuiv(kv.second, GL_QUERY_RESULT_AVAILABLE, &avail);
            if (avail) { GLuint got = 0; glGetQueryObjectuiv(kv.second, GL_QUERY_RESULT, &got); occVisible[kv.first] = got > 0 ? 1 : 0; }
        }

        // Opaque pass (frustum + occlusion culled).
        for (const tools::DrawItem& it : items)
        {
            glm::vec3 c(it.global[3]);
            if (!frustum.sphereInside(c, tools::worldRadius(it.global))) continue;
            auto ov = occVisible.find(it.node);
            if (ov != occVisible.end() && ov->second == 0) { lastOccluded++; continue; }
            if (it.node->material.alpha >= 1.0f)
            {
                glUniform1i(uSelected, it.node == selectedNode ? 1 : 0);
                tools::drawItem(it, u, viewProj);
                lastVisible++;
            }
        }

        instances.draw(viewProj, instanceTex, glm::normalize(dirToLight),
                       lightSpace, shadow.texture()); // cast + receive shadows

        // Occlusion pass: bounding-box depth-only queries (results read next frame).
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glUseProgram(occProg);
        glBindVertexArray(cube->vao);
        for (const tools::DrawItem& it : items)
        {
            glm::vec3 c(it.global[3]);
            float r = tools::worldRadius(it.global);
            if (!frustum.sphereInside(c, r)) continue;
            glm::mat4 box = glm::scale(glm::translate(glm::mat4(1.0f), c), glm::vec3(2.0f * r));
            glm::mat4 mvp = viewProj * box;
            glUniformMatrix4fv(occMVP, 1, GL_FALSE, &mvp[0][0]);
            GLuint& q = occQuery[it.node];
            if (!q) glGenQueries(1, &q);
            glBeginQuery(GL_ANY_SAMPLES_PASSED, q);
            glDrawElements(GL_TRIANGLES, cube->indexCount, GL_UNSIGNED_INT, (void*)0);
            glEndQuery(GL_ANY_SAMPLES_PASSED);
        }
        glBindVertexArray(0);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        skybox.draw(camera.view(), proj); // fills the background (depth == far)

        // Transparent pass: back-to-front, blended, no depth write.
        std::vector<const tools::DrawItem*> trans;
        for (const tools::DrawItem& it : items)
        {
            glm::vec3 c(it.global[3]);
            if (it.node->material.alpha < 1.0f && frustum.sphereInside(c, tools::worldRadius(it.global)))
            {
                trans.push_back(&it);
                lastVisible++;
            }
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
            for (const tools::DrawItem* it : trans)
            {
                glUniform1i(uSelected, it->node == selectedNode ? 1 : 0);
                tools::drawItem(*it, u, viewProj);
            }
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }

        // Resolve offscreen scene to the screen with post-processing
        // (motion blur / DoF / fog / bloom / tonemap / grain).
        glm::mat4 invVP = glm::inverse(viewProj);
        post.draw(width, height, k::PerspectiveNear, k::PerspectiveFar,
                  (float)animTime, invVP, prevViewProj);
        prevViewProj = viewProj;

        // HUD overlay: 2D text, no depth, no cull, alpha-blended.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        text.draw("smallgine  FPS " + std::to_string(currentFps) +
                  "  visible " + std::to_string(lastVisible) + "/" + std::to_string(lastTotal) +
                  "  occluded " + std::to_string(lastOccluded),
                  12.0f, 26.0f, width, height, glm::vec3(1.0f, 1.0f, 1.0f));
        text.draw("+", width * 0.5f - 5.0f, height * 0.5f + 6.0f, width, height, glm::vec3(1.0f)); // crosshair
        text.draw("WASD | F pick | arrows/[ ] move sel | C camera | N/M spawn | F5/F9 save/load | ESC",
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

        // Per-node behavior: keyframe animation + data-driven spin.
        animTime += dt;
        updateNode(scene.MainNode, (float)dt, (float)animTime);

        // Spatial audio: listener tracks the camera; emitter follows "left" node.
        audio.setListener(camera.position, camera.front, camera.up);
        if (Node* left = scene.MainNode.find("left"))
        {
            // approximate world pos: content offset + left local (content spins, ok as demo)
            audio.setEmitterPos(left->Position + glm::vec3(0.0f, 0.3f, 0.0f));
        }

        // FPS report once per second.
        fpsAccum += dt;
        fpsFrames++;
        if (fpsAccum >= 1.0)
        {
            currentFps = fpsFrames;
            std::cout << "FPS " << currentFps << " | visible " << lastVisible
                      << "/" << lastTotal << " | occluded " << lastOccluded << std::endl;
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
                case GLFW_KEY_C:  cycleCamera(); return;
                case GLFW_KEY_F:  pick();        return; // pick along crosshair
                default: break;
            }
        }

        // Editor gizmo: arrow keys / brackets move the selected node (press or repeat).
        if (action != GLFW_RELEASE)
        {
            const float s = k::GizmoStep;
            switch (key)
            {
                case GLFW_KEY_LEFT:          moveSelected({-s, 0, 0}); return;
                case GLFW_KEY_RIGHT:         moveSelected({ s, 0, 0}); return;
                case GLFW_KEY_UP:            moveSelected({0, 0, -s}); return;
                case GLFW_KEY_DOWN:          moveSelected({0, 0,  s}); return;
                case GLFW_KEY_RIGHT_BRACKET: moveSelected({0,  s, 0}); return;
                case GLFW_KEY_LEFT_BRACKET:  moveSelected({0, -s, 0}); return;
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

    void click(int button, int action)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) pick();
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
        for (auto& kv : occQuery) if (kv.second) glDeleteQueries(1, &kv.second);
        if (occProg) glDeleteProgram(occProg);
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
