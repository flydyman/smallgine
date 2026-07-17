#pragma once
#include <nlohmann/json.hpp>
#include <memory>
#include "gamemode.hpp"
#include "../platform/glcontext.hpp"
#include "../platform/paths.hpp"
#include "../platform/vfs.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include "constants.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "light.hpp"
#include "../tools/helpers.hpp"
#include "../tools/debugdraw.hpp"
#include "../tools/shader.hpp"
#include "../tools/glcheck.hpp"
#include "../tools/text.hpp"
#include "../tools/frustum.hpp"
#include "../tools/ui.hpp"
#include "../tools/navmesh.hpp"
#include "../tools/jobs.hpp"
#include "../tools/ecs.hpp"
#include "../tools/broadphase.hpp"
#include "../tools/profiler.hpp"
#include "../tools/capture.hpp"
#include "../tools/net.hpp"
#include <set>
#include "../resources/mesh.hpp"
#include "../resources/objloader.hpp"
#include "../resources/gltf.hpp"
#include "../resources/texture.hpp"
#include "../resources/skybox.hpp"
#include "../resources/shadow.hpp"
#include "../resources/pointshadow.hpp"
#include "../resources/csm.hpp"
#include "../resources/instanced.hpp"
#include "../resources/postfx.hpp"
#include "../resources/deferred.hpp"
#include "../resources/particles.hpp"
#include "../resources/terrain.hpp"
#include "../resources/skinned.hpp"
#include "../resources/skinnedgltf.hpp"
#include "../resources/decals.hpp"
#include "../resources/gpuparticles.hpp"
#include "../resources/script.hpp"
#include "../audio/audio.hpp"

namespace smallgine {

namespace {



    // Demo scene as JSON. Materials per node; leftChild (octa) inherits its .mtl.
    [[maybe_unused]] const char* kSceneJson = R"({
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
                    "name": "fountain",
                    "position": [0.0, 0.1, 0.0],
                    "emitter": { "count": 220, "colorA": [0.9, 0.4, 0.1], "colorB": [1.0, 0.85, 0.35], "size": 0.13, "speed": 2.8, "spread": 0.6, "gravity": 2.0, "life": 1.9 }
                },
                {
                    "name": "glbDemo",
                    "position": [-2.6, 0.6, 1.4],
                    "scale": [0.6, 0.6, 0.6],
                    "spin": [0.0, 45.0, 0.0],
                    "mesh": "assets/hier.glb"
                },
                {
                    "name": "faller",
                    "position": [2.2, 3.4, 0.4],
                    "scale": [0.4, 0.4, 0.4],
                    "dynamic": true,
                    "material": { "color": [0.4, 0.9, 1.0], "texture": "assets/test2.tga", "shininess": 40.0, "specular": 0.6 }
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
                            "material": { "color": [0.85, 0.88, 0.9], "texture": "assets/test2.tga", "metallic": 1.0, "roughness": 0.25 }
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
    GLint  uIsTerrain = -1, uRockColor = -1, uMetallic = -1, uRoughness = -1;
    GLint  uIsWater = -1, uTime = -1;
    GLint  uPointShadow = -1, uHasPointShadow = -1, uPointLightPos = -1, uPointFar = -1;
    GLint  uCSM = -1, uCSMMat = -1, uCSMSplit = -1, uEnv = -1;

    std::shared_ptr<Mesh> cube;
    std::map<std::string, LoadedModel> models;   // OBJ path -> mesh + material (cache)
    std::map<std::string, Texture> textures;     // path -> loaded GL texture (cache)
    std::string texturePath = "assets/test.tga"; // engine default texture
    std::string scenePath;                        // scene JSON file (config); empty => embedded
    std::string packPath = "assets.sgpk";         // asset pack mounted at startup if present
    Scene scene;
    Camera camera;
    std::vector<Camera> camPresets;
    int activeCam = 0;
    const Node* selectedNode = nullptr;
    std::string selectedName;
    AudioSystem audio;
    Skybox skybox;
    ShadowMap shadow;
    ShadowCube pointShadow;
    CascadedShadow csm;
    TextRenderer text;
    UI ui;
    bool editorOpen = false;
    InstancedField instances;
    GLuint instanceTex = 0;
    PostFX post;
    Deferred deferred;
    bool deferredMode = false; // opaque via G-buffer (contained; forward stays for the rest)
    int postW = 0, postH = 0;
    glm::mat4 prevViewProj{1.0f};
    int currentFps = 0;
    std::vector<Light> lights;

    std::vector<ParticleSystem> emitters;   // data-driven, one per emitter node
    SkinnedModel skinned;                   // procedural GPU-skinned demo
    SkinnedGLTF rigged;                      // imported rigged glTF (JOINTS/WEIGHTS/anim)
    std::shared_ptr<Mesh> terrainMesh;
    std::vector<float> terrainHeights;      // unit-space heights (grid x grid)
    int terrainGrid = 0;
    glm::vec3 terrainPos{0.0f}, terrainScale{1.0f};

    NavMesh nav;                            // A* over walkable terrain cells
    std::vector<glm::vec3> agentPath;
    size_t agentWp = 0;

    // sandbox=false strips the built-in engine props (the standalone demos set this).
    bool sandbox = true;

    // Pluggable demo gameplay. Null => engine sandbox with free-fly camera.
    std::unique_ptr<IGameMode> game;

    // Generic modifier-key state, read by game modes (e.g. space thrust/brake).
    bool keyShift = false, keyCtrl = false;

    NetSystem net;                          // UDP loopback state replication
    bool netOn = false;

    Profiler prof;                          // per-frame CPU section timings (overlay)
    bool captureReq = false;                // screenshot requested this frame
    int shotCount = 0;

    JobSystem jobs;                         // thread pool (parallel culling / ECS)
    EcsWorld ecs;                           // component-system world alongside the scene graph
    std::vector<char> visible;              // per-item frustum visibility (job-filled)

    GpuParticles gpuParticles;              // compute-shader particle sim
    bool gpuFxOn = true;
    ScriptSystem script;                    // LuaJIT node behaviors

    Decals decals;                          // projected ground splats
    struct Trigger { glm::vec3 center, half; std::string name; bool camIn = false, agentIn = false; };
    std::vector<Trigger> triggers;          // AABB event zones
    bool hasPointLight = false;
    glm::vec3 pointLightPos{0.0f};

    // Split-screen: render the scene from two cameras side by side (no post-fx).
    bool splitScreen = false;

    // Debug overlay: draw each node's world-space collider AABB (the same box the
    // physics/broadphase test) as an x-ray green wireframe. Toggled with F3.
    bool        showColliders = false;
    DebugLines  debugLines;

    // Simple rigid bodies (gravity + ground rest + pairwise AABB) under "physicsRoot".
    struct Body { std::string name; glm::vec3 vel{0.0f}; float half = 0.15f; };
    std::vector<Body> bodies;
    int physCount = 0;

    // Right-mouse-drag gizmo: translate selected node in the camera plane.
    bool rmbDown = false;

    // Shader hot-reload: poll lit.vert/lit.frag mtimes and relink on change.
    long litVertMtime = 0, litFragMtime = 0;
    double shaderPoll = 0.0;
    std::map<std::string, long> assetMtime; // hot-reload watch: textures + scene file

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

    void ensurePickBuffer(int w, int h)
    {
        if (pickFbo && pickW == w && pickH == h) return;
        pickW = w; pickH = h;
        if (!pickTex) glGenTextures(1, &pickTex);
        glBindTexture(GL_TEXTURE_2D, pickTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!pickRbo) glGenRenderbuffers(1, &pickRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, pickRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        if (!pickFbo) glGenFramebuffers(1, &pickFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, pickFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pickRbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Pixel-accurate pick: render node IDs, read the pixel under the crosshair.
    void pick()
    {
        if (postW == 0) return; // needs a framebuffer size from a prior frame
        ensurePickBuffer(postW, postH);

        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        float aspect = pickH > 0 ? (float)pickW / (float)pickH : 1.0f;
        glm::mat4 vp = glm::perspective(glm::radians(camera.fov), aspect,
                                        k::PerspectiveNear, k::PerspectiveFar) * camera.view();

        glBindFramebuffer(GL_FRAMEBUFFER, pickFbo);
        glViewport(0, 0, pickW, pickH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(pickProg);
        for (size_t i = 0; i < items.size(); ++i)
        {
            glm::mat4 mvp = vp * items[i].global;
            glUniformMatrix4fv(pickMVP, 1, GL_FALSE, &mvp[0][0]);
            unsigned id = (unsigned)i + 1; // 0 = background
            glUniform3f(pickID, (id & 255) / 255.0f, ((id >> 8) & 255) / 255.0f, ((id >> 16) & 255) / 255.0f);
            items[i].node->mesh->draw();
        }
        unsigned char px[4] = {0, 0, 0, 0};
        glReadPixels(pickW / 2, pickH / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        unsigned id = px[0] | (px[1] << 8) | (px[2] << 16);
        const Node* best = (id > 0 && id <= items.size()) ? items[id - 1].node : nullptr;
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

    // Editor panel buttons (labels reflect live toggle state).
    enum EBtn { BtnSave, BtnReload, BtnSpawn, BtnDespawn, BtnScaleUp, BtnScaleDown,
                BtnSSAO, BtnSSR, BtnDefer, BtnSplit };
    std::vector<UIButton> editorButtons(int w, int /*h*/)
    {
        std::vector<UIButton> b;
        float x = w - 210.0f, y = 92.0f, bw = 196.0f, bh = 26.0f, sp = 30.0f;
        auto add = [&](const std::string& l, int id) { b.push_back({{x, y, bw, bh}, l, id}); y += sp; };
        add("Save scene", BtnSave);
        add("Reload scene", BtnReload);
        add("Spawn cube", BtnSpawn);
        add("Despawn cube", BtnDespawn);
        add("Scale + (sel)", BtnScaleUp);
        add("Scale - (sel)", BtnScaleDown);
        add(std::string("SSAO: ") + (post.ssaoEnabled() ? "on" : "off"), BtnSSAO);
        add(std::string("SSR: ") + (post.ssrEnabled() ? "on" : "off"), BtnSSR);
        add(std::string("Deferred: ") + (deferredMode ? "on" : "off"), BtnDefer);
        add(std::string("Split: ") + (splitScreen ? "on" : "off"), BtnSplit);
        return b;
    }
    void doEditorAction(int id)
    {
        switch (id)
        {
            case BtnSave:      saveScene();               break;
            case BtnReload:    reloadScene();             break;
            case BtnSpawn:     spawnNode();               break;
            case BtnDespawn:   despawnNode();             break;
            case BtnScaleUp:   scaleSelected(1.1f);       break;
            case BtnScaleDown: scaleSelected(0.9f);       break;
            case BtnSSAO:      post.toggleSSAO();         break;
            case BtnSSR:       post.toggleSSR();          break;
            case BtnDefer:     deferredMode = !deferredMode; break;
            case BtnSplit:     splitScreen = !splitScreen;   break;
            default: break;
        }
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
        physCount = 0;
        buildEmitters();
        bodies.clear();
        registerDynamic(scene.MainNode);
        std::cout << "Scene reloaded: '" << scene.name << "'" << std::endl;
    }

    double animTime = 0.0;
    int lastVisible = 0, lastTotal = 0, lastOccluded = 0;

    // Pixel-accurate picking via an ID pass (encode item index into a color buffer).
    GLuint pickFbo = 0, pickTex = 0, pickRbo = 0, pickProg = 0;
    GLint pickMVP = -1, pickID = -1;
    int pickW = 0, pickH = 0;

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

        Texture t = loadTexture(key);
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
        bool isGltf = (path.size() > 5 && path.substr(path.size() - 5) == ".gltf") ||
                      (path.size() > 4 && path.substr(path.size() - 4) == ".glb");
        lm.mesh = isGltf ? loadGLTF(path, &m, &has) : loadOBJ(path, &m, &has);
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
        else if (!n.Children.empty() && !n.materialSet && !n.Emitter.enabled)
        {
            n.mesh = nullptr; // pure transform group (pivot): no default cube
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

    // World-space position of a node by name (accumulates parent transforms).
    bool worldPosOf(const Node& n, const glm::mat4& parent, const std::string& name, glm::vec3& out)
    {
        glm::mat4 global = parent * n.localMatrix();
        if (n.Name == name) { out = glm::vec3(global[3]); return true; }
        for (const Node& c : n.Children) if (worldPosOf(c, global, name, out)) return true;
        return false;
    }

    // Collect emitter nodes into ParticleSystems (called after the scene loads).
    void collectEmitterNames(const Node& n, std::vector<std::pair<std::string, EmitterSpec>>& out)
    {
        if (n.Emitter.enabled) out.push_back({n.Name, n.Emitter});
        for (const Node& c : n.Children) collectEmitterNames(c, out);
    }
    void buildEmitters()
    {
        for (ParticleSystem& p : emitters) p.free();
        emitters.clear();
        std::vector<std::pair<std::string, EmitterSpec>> specs;
        collectEmitterNames(scene.MainNode, specs);
        for (auto& s : specs)
        {
            glm::vec3 pos(0.0f);
            worldPosOf(scene.MainNode, glm::mat4(1.0f), s.first, pos);
            emitters.emplace_back();
            emitters.back().init(pos, s.second);
        }
        std::cout << "Emitters: " << emitters.size() << std::endl;
    }

    // Query all uniform locations for the lit program (called on link + hot-reload).
    void fetchUniforms()
    {
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
        uIsTerrain = glGetUniformLocation(program, "uIsTerrain");
        uRockColor = glGetUniformLocation(program, "uRockColor");
        uMetallic = glGetUniformLocation(program, "uMetallic");
        uRoughness = glGetUniformLocation(program, "uRoughness");
        uIsWater = glGetUniformLocation(program, "uIsWater");
        uTime = glGetUniformLocation(program, "uTime");
        uPointShadow = glGetUniformLocation(program, "uPointShadow");
        uHasPointShadow = glGetUniformLocation(program, "uHasPointShadow");
        uPointLightPos = glGetUniformLocation(program, "uPointLightPos");
        uPointFar = glGetUniformLocation(program, "uPointFar");
        uCSM = glGetUniformLocation(program, "uCSM");
        uCSMMat = glGetUniformLocation(program, "uCSMMat");
        uCSMSplit = glGetUniformLocation(program, "uCSMSplit");
        uEnv = glGetUniformLocation(program, "uEnv");
    }

    // Read the presented frame back and write a timestamped TGA next to the exe.
    void captureScreenshot(int w, int h)
    {
        std::vector<unsigned char> px((size_t)w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        std::string path = resolvePath("shot_" + std::to_string(shotCount++) + ".tga");
        if (writeTGA(path, w, h, px.data())) std::cout << "Screenshot: " << path << std::endl;
    }

    // Hot-reload watch for non-shader assets: textures (re-uploaded in place so
    // cached node ids stay valid) and the saved scene file.
    void watchAssets()
    {
        for (auto& kv : textures)
        {
            std::string path = resolvePath(kv.first);
            long mt = tools::fileMtime(path);
            auto it = assetMtime.find(kv.first);
            if (it == assetMtime.end()) { assetMtime[kv.first] = mt; continue; }
            if (mt && mt != it->second) { it->second = mt; reloadTexture(kv.second.id, kv.first); }
        }
        std::string sp = resolvePath("scene_saved.json");
        long smt = tools::fileMtime(sp);
        auto sit = assetMtime.find("__scene");
        if (sit == assetMtime.end()) assetMtime["__scene"] = smt;
        else if (smt && smt != sit->second) { sit->second = smt; reloadScene(); }
        script.reloadIfChanged(); // Lua behavior hot-reload
    }

    // Hot-reload: relink the lit program from disk, keeping the old one on failure.
    void reloadShaders()
    {
        GLuint np = tools::linkProgramFiles("assets/shaders/lit.vert", "assets/shaders/lit.frag");
        if (np == 0) { std::cout << "Shader reload failed (kept previous)" << std::endl; return; }
        if (program) glDeleteProgram(program);
        program = np;
        fetchUniforms();
        std::cout << "Shaders reloaded" << std::endl;
    }

    // Rotate/scale the selected node (editor transform).
    void rotateSelected(const glm::vec3& deg)
    {
        if (Node* n = selectedName.empty() ? nullptr : scene.MainNode.find(selectedName)) n->Rotation += deg;
    }
    void scaleSelected(float factor)
    {
        if (Node* n = selectedName.empty() ? nullptr : scene.MainNode.find(selectedName)) n->Scale *= factor;
    }

    // Spawn a falling rigid body at the camera focus; gravity + ground/AABB in stepPhysics.
    void spawnPhysics()
    {
        Node* pr = scene.MainNode.find("physicsRoot");
        if (!pr) return;
        Node n;
        n.Name = "phys" + std::to_string(physCount);
        auto rnd = [](float a, float b) { return a + (b - a) * (float)(std::rand() % 1000) / 1000.0f; };
        n.Position = glm::vec3(rnd(-1.5f, 1.5f), 3.5f, rnd(-1.5f, 1.5f));
        float sc = rnd(0.28f, 0.42f);
        n.Scale = glm::vec3(sc);
        n.material.color = glm::vec3(rnd(0.3f, 1.0f), rnd(0.3f, 1.0f), rnd(0.3f, 1.0f));
        n.material.texture = "assets/test.tga";
        n.materialSet = true;
        pr->addChild(n);
        prepareNodes(*pr);
        bodies.push_back({n.Name, glm::vec3(rnd(-1.0f, 1.0f), 0.0f, rnd(-1.0f, 1.0f)), sc * 0.5f});
        physCount++;
        std::cout << "Physics body " << n.Name << " dropped" << std::endl;
    }

    // Ground surface height under a world XZ (terrain if present, else flat slab).
    float surfaceY(float x, float z)
    {
        bool onT = false;
        float t = terrainHeightAt(x, z, onT);
        return onT ? t : k::PhysGroundTop;
    }

    // Fire triggers on camera/agent entry; drop a decal at the entry point.
    void updateTriggers()
    {
        Node* agent = scene.MainNode.find("agent");
        auto test = [&](Trigger& t, const glm::vec3& p, bool& flag, const char* who) {
            bool in = std::fabs(p.x - t.center.x) < t.half.x &&
                      std::fabs(p.y - t.center.y) < t.half.y &&
                      std::fabs(p.z - t.center.z) < t.half.z;
            if (in && !flag)
            {
                std::cout << who << " entered trigger '" << t.name << "'" << std::endl;
                decals.add(glm::vec3(p.x, surfaceY(p.x, p.z), p.z), 1.3f);
            }
            flag = in;
        };
        for (Trigger& t : triggers)
        {
            test(t, camera.position, t.camIn, "Camera");
            if (agent) test(t, agent->Position, t.agentIn, "Agent");
        }
    }

    // Pick a random walkable goal and A*-path the agent there.
    void navGoto()
    {
        Node* a = scene.MainNode.find("agent");
        if (!a || nav.size() == 0) return;
        // Try random walkable goals until one is reachable from the agent.
        agentPath.clear();
        for (int t = 0; t < 40 && agentPath.empty(); ++t)
        {
            int cx = std::rand() % nav.size(), cz = std::rand() % nav.size();
            if (!nav.walkable(cx, cz)) continue;
            agentPath = nav.findPath(a->Position, nav.cellCenter(cx, cz));
        }
        agentWp = agentPath.empty() ? 0 : 1;
        std::cout << "Nav path: " << agentPath.size() << " waypoints" << std::endl;
    }

    // Advance the agent along its path, snapping to the terrain surface.
    void stepAgent(float dt)
    {
        if (agentPath.empty() || agentWp >= agentPath.size()) return;
        Node* a = scene.MainNode.find("agent");
        if (!a) return;
        glm::vec3 d = agentPath[agentWp] - a->Position; d.y = 0.0f;
        float dist = glm::length(d);
        float step = 2.0f * dt;
        if (dist <= step) { a->Position = agentPath[agentWp]; ++agentWp; }
        else a->Position += d / dist * step;
        bool onT = false;
        a->Position.y = terrainHeightAt(a->Position.x, a->Position.z, onT) + a->Scale.y * 0.5f;
    }

    // Terrain surface height at a world XZ (bilinear). onT=false when off the mesh.
    float terrainHeightAt(float wx, float wz, bool& onT) const
    {
        onT = false;
        if (terrainGrid <= 0 || terrainHeights.empty()) return PhysGroundTopValue();
        float fx = (wx - terrainPos.x) / terrainScale.x + 0.5f;
        float fz = (wz - terrainPos.z) / terrainScale.z + 0.5f;
        if (fx < 0.0f || fx > 1.0f || fz < 0.0f || fz > 1.0f) return PhysGroundTopValue();
        float gx = fx * (terrainGrid - 1), gz = fz * (terrainGrid - 1);
        int x0 = (int)gx, z0 = (int)gz;
        int x1 = std::min(x0 + 1, terrainGrid - 1), z1 = std::min(z0 + 1, terrainGrid - 1);
        float tx = gx - x0, tz = gz - z0;
        auto H = [&](int x, int z) { return terrainHeights[z * terrainGrid + x]; };
        float h = H(x0, z0) * (1 - tx) * (1 - tz) + H(x1, z0) * tx * (1 - tz)
                + H(x0, z1) * (1 - tx) * tz + H(x1, z1) * tx * tz;
        onT = true;
        return terrainPos.y + h * terrainScale.y; // world-space surface top
    }
    static constexpr float PhysGroundTopValue() { return k::PhysGroundTop; }

    // Register scene nodes flagged `dynamic` as rigid bodies (called on load/reload).
    void registerDynamic(Node& n)
    {
        if (n.Dynamic)
            bodies.push_back({n.Name, glm::vec3(0.0f), std::max(n.Scale.x, n.Scale.z) * 0.5f});
        for (Node& c : n.Children) registerDynamic(c);
    }

    // Integrate rigid bodies: gravity, terrain/ground rest, pairwise AABB separation.
    void stepPhysics(float dt)
    {
        if (bodies.empty()) return;
        for (Body& b : bodies)
        {
            Node* n = scene.MainNode.find(b.name);
            if (!n) continue;
            b.vel.y -= k::PhysGravity * dt;
            n->Position += b.vel * dt;
            bool onT = false;
            float surf = terrainHeightAt(n->Position.x, n->Position.z, onT);
            float groundTop = std::max(k::PhysGroundTop, surf); // terrain wins where present
            if (n->Position.y - b.half < groundTop)
            {
                n->Position.y = groundTop + b.half;
                b.vel.y = -b.vel.y * k::PhysRestitution;
                b.vel.x *= k::PhysFriction; b.vel.z *= k::PhysFriction;
                if (std::fabs(b.vel.y) < k::PhysSleep) b.vel.y = 0.0f; // settle
            }
        }
        // Broadphase collision: bodies + static scene boxes binned in a spatial hash,
        // so each body only tests nearby candidates. Bodies land on/against statics.
        struct Col { AABB box; glm::vec3 half; int body; Node* node; };
        std::vector<Col> cols;

        std::set<std::string> bodyNames;
        for (const Body& b : bodies) bodyNames.insert(b.name);
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            Node* n = scene.MainNode.find(bodies[i].name);
            if (!n) continue;
            glm::vec3 half(bodies[i].half);
            cols.push_back({ AABB::fromCenter(n->Position, half), half, (int)i, n });
        }
        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (bodyNames.count(nd->Name) || nd->Name == "ground" || nd->Name == "terrain"
                || nd->material.alpha < 1.0f) continue;
            glm::vec3 half(0.5f * glm::length(glm::vec3(it.global[0])),
                           0.5f * glm::length(glm::vec3(it.global[1])),
                           0.5f * glm::length(glm::vec3(it.global[2])));
            cols.push_back({ AABB::fromCenter(glm::vec3(it.global[3]), half), half, -1, nullptr });
        }

        SpatialHash grid;
        grid.reset(1.0f);
        for (size_t i = 0; i < cols.size(); ++i) grid.insert((int)i, cols[i].box);

        std::vector<int> cand;
        for (size_t i = 0; i < cols.size(); ++i)
        {
            if (cols[i].body < 0) continue; // resolve dynamic bodies only
            grid.query(cols[i].box, (int)i, cand);
            for (int cid : cand)
            {
                Col& A = cols[i]; Col& B = cols[cid];
                if (!A.box.overlaps(B.box)) continue;
                float px = std::min(A.box.max.x - B.box.min.x, B.box.max.x - A.box.min.x);
                float py = std::min(A.box.max.y - B.box.min.y, B.box.max.y - A.box.min.y);
                float pz = std::min(A.box.max.z - B.box.min.z, B.box.max.z - A.box.min.z);
                int ax = (px < py && px < pz) ? 0 : (py < pz ? 1 : 2);
                float pen = (ax == 0) ? px : (ax == 1) ? py : pz;
                glm::vec3 ac = (A.box.min + A.box.max) * 0.5f, bc = (B.box.min + B.box.max) * 0.5f;
                float sign = (ac[ax] < bc[ax]) ? -1.0f : 1.0f;      // push A to its own side
                float move = (B.body < 0 ? pen : pen * 0.5f) * sign; // full vs static, half vs dynamic
                Body& body = bodies[A.body];
                A.node->Position[ax] += move;
                A.box = AABB::fromCenter(A.node->Position, A.half);   // keep box current
                if (ax == 1 && sign > 0.0f) { if (body.vel.y < 0.0f) body.vel.y = 0.0f; } // rest on top
                else if (ax == 1) body.vel.y = -body.vel.y * k::PhysRestitution;
                else body.vel[ax] *= -0.3f;
            }
        }
    }

public:
    void setWindow(GLFWwindow* w) { window = w; }

    // Install a demo's game mode (its setup() runs during create()).
    void setGameMode(std::unique_ptr<IGameMode> m) { game = std::move(m); }

    // --- API surface for game modes ---------------------------------------
    Scene&        gameScene()  { return scene; }
    Camera&       gameCamera() { return camera; }
    AudioSystem&  gameAudio()  { return audio; }
    UI&           gameUI()     { return ui; }
    TextRenderer& gameText()   { return text; }
    std::shared_ptr<Mesh> gameCube() { return cube; }
    // Load (or fetch cached) a mesh + its material from an OBJ/glTF asset path.
    // Falls back to the cube mesh on failure (mesh == gameCube()). Requires a GL
    // context, so call it from a game mode's setup()/update(), not before.
    LoadedModel& gameModel(const std::string& p) { return modelFor(p); }
    GLuint gameTexture(const std::string& p) { return textureFor(p); }
    float  gameGroundY(float x, float z) { return surfaceY(x, z); }
    void   gamePick() { pick(); }
    const std::string& gameSelected() const { return selectedName; }
    int    gameScreenW() const { return postW; }
    int    gameScreenH() const { return postH; }
    double gameMouseX() const { return lastX; }
    double gameMouseY() const { return lastY; }
    bool kW() const { return keyW; } bool kS() const { return keyS; }
    bool kA() const { return keyA; } bool kD() const { return keyD; }
    bool kQ() const { return keyQ; } bool kE() const { return keyE; }
    bool kShift() const { return keyShift; } bool kCtrl() const { return keyCtrl; }
    void gameCaptureMouse(bool on)
    {
        mouseCaptured = on;
        if (window) glfwSetInputMode(window, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }

    void configure(const Config& cfg)
    {
        camera.speed = cfg.cameraSpeed;
        camera.sensitivity = cfg.mouseSensitivity;
        texturePath = cfg.texture;
        scenePath = cfg.scene;
        packPath = cfg.pack;
        sandbox = cfg.sandbox;
        gpuFxOn = cfg.sandbox; // GPU fountain is sandbox dressing
    }

    Engine()
    {
        isInitialized = true;
        isLooped = false;
        std::cout << "Engine created" << std::endl;
    }

    bool looped() { return isLooped; }

    void create();

    void uploadLights();

    // Forward render of the whole scene from one camera into the bound framebuffer.
    // Occlusion queries only run for the primary view (they use shared query objects).
    void renderForward(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos,
                       const glm::vec3& dirToLight, const glm::mat4& lightSpace,
                       std::vector<tools::DrawItem>& items, bool doOcclusion, bool drawOpaque = true);

    void draw(GLFWwindow* window);

    void process(double dt);

    void input(int key, int /*scancode*/, int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            isLooped = false;
            return;
        }

        // Game modes get every discrete key press (held movement is polled separately).
        if (action == GLFW_PRESS && game) game->onKeyPress(*this, key);

        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        {
            if (!game) audio.playTone(660.0f, 0.15f);
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
                case GLFW_KEY_P:  spawnPhysics(); return; // drop a rigid body
                case GLFW_KEY_K:  navGoto();     return; // A*-path the agent
                case GLFW_KEY_R:  reloadShaders(); return; // force shader reload
                case GLFW_KEY_V:  splitScreen = !splitScreen;
                                  std::cout << "Split-screen: " << (splitScreen ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_O:  post.toggleSSAO();
                                  std::cout << "SSAO: " << (post.ssaoEnabled() ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_L:  post.toggleSSR();
                                  std::cout << "SSR: " << (post.ssrEnabled() ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_G:  deferredMode = !deferredMode;
                                  std::cout << "Deferred (opaque): " << (deferredMode ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_T:  audio.toggleReverb(); return;
                case GLFW_KEY_Y:  prof.toggle();
                                  std::cout << "Profiler: " << (prof.on() ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_F3: showColliders = !showColliders;
                                  std::cout << "Show colliders: " << (showColliders ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_F12: captureReq = true; return; // screenshot
                case GLFW_KEY_H:  gpuFxOn = !gpuFxOn;
                                  std::cout << "GPU particles: " << (gpuFxOn ? "on" : "off") << std::endl;
                                  return;
                case GLFW_KEY_J:  if (!net.active()) net.start();
                                  if (net.active()) netOn = !netOn;
                                  std::cout << "Networking: " << (netOn ? "on (UDP sync)" : "off") << std::endl;
                                  return;
                case GLFW_KEY_I:  editorOpen = !editorOpen;
                                  mouseCaptured = !editorOpen;
                                  if (window) glfwSetInputMode(window, GLFW_CURSOR,
                                      editorOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                                  firstMouse = true;
                                  std::cout << "Editor: " << (editorOpen ? "open" : "closed") << std::endl;
                                  return;
                default: break;
            }
        }

        // Editor gizmo: arrow keys move, brackets lift, ,/. rotate, -/= scale (press or repeat).
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
                case GLFW_KEY_COMMA:         rotateSelected({0, -6, 0}); return;
                case GLFW_KEY_PERIOD:        rotateSelected({0,  6, 0}); return;
                case GLFW_KEY_MINUS:         scaleSelected(0.95f); return;
                case GLFW_KEY_EQUAL:         scaleSelected(1.05f); return;
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
            case GLFW_KEY_LEFT_SHIFT:   keyShift = held; break; // space demo: thrust
            case GLFW_KEY_LEFT_CONTROL: keyCtrl  = held; break; // space demo: brake
            default: break;
        }
    }

    void scroll(double yoffset)
    {
        camera.addZoom((float)yoffset);
    }

    void click(int button, int action)
    {
        if (game && game->onClick(*this, button, action)) return; // mode consumed it

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            // Editor panel consumes clicks over its buttons.
            if (editorOpen)
            {
                for (const UIButton& b : editorButtons(postW, postH))
                    if (b.rect.contains((float)lastX, (float)lastY)) { doEditorAction(b.id); return; }
            }
            pick();
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) rmbDown = (action == GLFW_PRESS);
    }

    void mouse(double xpos, double ypos)
    {
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

        // Right-drag translates the selected node in the camera plane (gizmo).
        if (rmbDown && !selectedName.empty())
        {
            glm::vec3 delta = camera.right() * (dx * 0.01f) + camera.up * (dy * 0.01f);
            moveSelected(delta);
            return;
        }

        if (!mouseCaptured) return;
        camera.addLook(dx, dy);
    }

    void cleanup()
    {
        if (game) game->teardown(*this);
        jobs.stop();
        net.stop();
        audio.cleanup();
        skybox.free();
        shadow.free();
        pointShadow.free();
        csm.free();
        text.free();
        instances.free();
        post.free();
        deferred.free();
        for (ParticleSystem& p : emitters) p.free();
        skinned.free();
        rigged.free();
        ui.free();
        decals.free();
        gpuParticles.free();
        script.free();
        if (terrainMesh) terrainMesh->free();
        for (auto& kv : occQuery) if (kv.second) glDeleteQueries(1, &kv.second);
        if (occProg) glDeleteProgram(occProg);
        if (pickProg) glDeleteProgram(pickProg);
        if (pickFbo) glDeleteFramebuffers(1, &pickFbo);
        if (pickTex) glDeleteTextures(1, &pickTex);
        if (pickRbo) glDeleteRenderbuffers(1, &pickRbo);
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
