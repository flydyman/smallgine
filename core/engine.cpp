// Out-of-line definitions for the heaviest Engine methods. Keeping these here
// (rather than inline in engine.hpp) means game.cpp no longer recompiles them
// on unrelated header edits, and this TU builds in parallel.
#include "engine.hpp"
#include <chrono>

namespace smallgine {

void Engine::create()
{
    isLooped = true;

    // Mount the asset pack if one sits next to the executable. Loaders then read
    // through the VFS (platform/vfs.hpp), pulling bytes from the pack and falling
    // back to loose files, so a packed build needs no asset folder shipped.
    if (!packPath.empty()) mountPack(resolvePath(packPath));

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
    fetchUniforms();
    litVertMtime = tools::fileMtime(resolvePath("assets/shaders/lit.vert"));
    litFragMtime = tools::fileMtime(resolvePath("assets/shaders/lit.frag"));

    cube = makeCube();
    occProg = tools::linkProgramFiles("assets/shaders/occ.vert", "assets/shaders/empty.frag");
    occMVP = glGetUniformLocation(occProg, "uMVP");
    pickProg = tools::linkProgramFiles("assets/shaders/id.vert", "assets/shaders/id.frag");
    pickMVP = glGetUniformLocation(pickProg, "uMVP");
    pickID = glGetUniformLocation(pickProg, "uID");
    skybox.init();
    shadow.init(k::ShadowMapSize);
    pointShadow.init(512);
    csm.init(1024);
    text.init("assets/font.ttf", 22.0f);
    ui.init();
    debugLines.init();

    // Scene from config file if set, else the built-in demo.
    bool loaded = false;
    if (!scenePath.empty())
    {
        std::string sj = readAssetText(scenePath);
        if (!sj.empty())
        {
            nlohmann::json j = nlohmann::json::parse(sj);
            scene = j.get<Scene>();
            loaded = true;
            std::cout << "Scene from file: " << scenePath << std::endl;
        }
        else std::cout << "Scene file missing (" << scenePath << "), using built-in" << std::endl;
    }
    if (!loaded) scene = loadSceneFromString(kSceneJson);
    prepareNodes(scene.MainNode);
    tools::glCheck("resource setup");

    // Baked sandbox props (terrain, nav agent, script/net nodes, ECS swarm, GPU
    // fountain, instanced ring). A demo run with "sandbox": false gets a clean
    // stage defined purely by its scene JSON.
    if (sandbox) {

    // Terrain: grid mesh from the height texture, added as a runtime-meshed node.
    terrainGrid = 96;
    terrainMesh = makeTerrain("assets/height.tga", terrainGrid, 1.0f, &terrainHeights);
    terrainPos = glm::vec3(0.0f, -1.55f, -6.5f);
    terrainScale = glm::vec3(11.0f, 2.2f, 11.0f);
    {
        Node t;
        t.Name = "terrain";
        t.Position = terrainPos;
        t.Scale = terrainScale;
        t.material.color = glm::vec3(0.42f, 0.55f, 0.32f); // grass (flat)
        t.material.rockColor = glm::vec3(0.34f, 0.30f, 0.26f); // rock (steep)
        t.material.terrain = true;
        t.material.texture = "assets/test.tga";
        t.material.shininess = 12.0f;
        t.material.specular = 0.15f;
        t.materialSet = true;
        t.mesh = terrainMesh;
        t.texId = textureFor(t.material.texture);
        scene.MainNode.Children.push_back(t);
    }

    // Nav mesh over walkable terrain + an agent that paths across it (K key).
    nav.build(terrainHeights, terrainGrid, terrainPos, terrainScale, 0.72f);
    std::cout << "NavMesh walkable cells: " << nav.walkableCount()
              << "/" << (terrainGrid * terrainGrid) << std::endl;
    {
        glm::vec3 start = terrainPos;
        long bestD = 1L << 60; int cc = nav.size() / 2;
        for (int z = 0; z < nav.size(); ++z)
            for (int x = 0; x < nav.size(); ++x)
                if (nav.walkable(x, z))
                {
                    long d = (long)(x - cc) * (x - cc) + (long)(z - cc) * (z - cc);
                    if (d < bestD) { bestD = d; start = nav.cellCenter(x, z); }
                }
        Node a;
        a.Name = "agent";
        a.Scale = glm::vec3(0.28f);
        a.Position = start + glm::vec3(0.0f, 0.14f, 0.0f);
        a.material.color = glm::vec3(1.0f, 0.2f, 0.15f);
        a.material.texture = "assets/test.tga";
        a.materialSet = true;
        a.mesh = cube;
        a.texId = textureFor("assets/test.tga");
        scene.MainNode.Children.push_back(a);
    }

    } // if (sandbox) — terrain / nav / agent

    // Empty container for runtime physics bodies (no mesh => not drawn itself).
    {
        Node pr;
        pr.Name = "physicsRoot";
        scene.MainNode.Children.push_back(pr);
    }

    if (sandbox) {
    // Lua-scripted node (behaviors.lua drives its transform each frame).
    {
        Node ln; ln.Name = "luaNode"; ln.Scale = glm::vec3(0.32f);
        ln.material.color = glm::vec3(1.0f, 0.85f, 0.2f); ln.material.texture = "assets/test2.tga"; ln.materialSet = true;
        ln.material.metallic = 0.6f; ln.material.roughness = 0.3f;
        ln.mesh = cube; ln.texId = textureFor("assets/test2.tga"); ln.Position = glm::vec3(0.0f, 2.0f, 0.0f);
        scene.MainNode.Children.push_back(ln);
    }

    // Networking demo: an orbiting source cube + a ghost the client drives over UDP.
    {
        Node src; src.Name = "netsrc"; src.Scale = glm::vec3(0.3f);
        src.material.color = glm::vec3(0.2f, 0.9f, 1.0f); src.material.texture = "assets/test.tga"; src.materialSet = true;
        src.mesh = cube; src.texId = textureFor("assets/test.tga"); src.Position = glm::vec3(2.2f, 2.6f, 0.0f);
        scene.MainNode.Children.push_back(src);
        Node gh; gh.Name = "netghost"; gh.Scale = glm::vec3(0.3f);
        gh.material.color = glm::vec3(1.0f, 0.3f, 0.9f); gh.material.texture = "assets/test.tga"; gh.materialSet = true;
        gh.mesh = cube; gh.texId = textureFor("assets/test.tga"); gh.Position = glm::vec3(2.2f, 3.3f, 0.0f);
        scene.MainNode.Children.push_back(gh);
    }

    // GPU-skinned demos: procedural + imported rigged glTF.
    skinned.init(glm::vec3(2.9f, -1.5f, 0.8f));
    rigged.load("assets/rig.glb", glm::vec3(-2.9f, -1.5f, 0.8f));

    // LuaJIT behavior script bound to the scene.
    script.init("assets/behaviors.lua", &scene);

    } // if (sandbox) — script/net nodes + skinned models

    // Thread pool (always up: parallel culling/ECS movement use it).
    jobs.start(0);
    std::cout << "JobSystem workers: " << jobs.workerCount() << std::endl;

    // ECS swarm of bouncing/spinning cubes (sandbox dressing).
    if (sandbox) {
        GLuint etex = textureFor("assets/test2.tga");
        auto rnd = [](float a, float b) { return a + (b - a) * (float)(std::rand() % 1000) / 1000.0f; };
        for (int i = 0; i < 48; ++i)
        {
            EcsWorld::Transform t; t.pos = { rnd(-4, 4), rnd(-1, 2.5f), rnd(-4, 4) }; t.scale = glm::vec3(rnd(0.12f, 0.24f));
            EcsWorld::Velocity v; v.v = { rnd(-1.4f, 1.4f), rnd(-1.0f, 1.0f), rnd(-1.4f, 1.4f) };
            EcsWorld::Spin s; s.degPerSec = { rnd(-90, 90), rnd(-90, 90), rnd(-90, 90) };
            EcsWorld::Renderable r; r.mesh = cube; r.tex = etex; r.color = glm::vec3(rnd(0.3f, 1.0f), rnd(0.3f, 1.0f), rnd(0.3f, 1.0f));
            ecs.add(t, v, s, r);
        }
    }

    // Compute-shader particle fountain (GPU-simulated, ~4k particles).
    if (sandbox) gpuParticles.init(4096, glm::vec3(1.7f, -1.4f, 1.6f), -1.5f);

    // Ground decals (always ready) + trigger volumes (sandbox only).
    decals.init();
    if (sandbox) {
        triggers.push_back({ glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(1.6f, 2.2f, 1.6f), "plaza", false, false });
        triggers.push_back({ glm::vec3(0.0f, 0.0f, -6.5f), glm::vec3(2.5f, 3.0f, 2.5f), "hilltop", false, false });
    }

    // Data-driven particle emitters: one system per node carrying an emitter.
    buildEmitters();

    // Register scene nodes flagged `dynamic` as physics bodies.
    bodies.clear();
    registerDynamic(scene.MainNode);

    // Instanced ring of small cubes around the scene (one draw call).
    if (sandbox) {
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
        audio.loadEmitter("assets/blip.wav", glm::vec3(-1.1f, 0.3f, 0.3f));
    }

    // Demo objects + gameplay are supplied entirely by the installed game mode.
    if (game) game->setup(*this);

    std::cout << "Engine initialized: scene '" << scene.name << "', "
              << lights.size() << " lights" << std::endl;
}

void Engine::uploadLights()
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

void Engine::renderForward(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos,
                           const glm::vec3& dirToLight, const glm::mat4& lightSpace,
                           std::vector<tools::DrawItem>& items, bool doOcclusion, bool drawOpaque)
{
    glm::mat4 viewProj = proj * view;

    glUseProgram(program);
    glUniform1i(uTex, 0);
    glUniform1i(uShadowMap, 1);
    glUniform1i(uNormalMap, 2);
    glUniform1i(uHeightMap, 3);
    glUniformMatrix4fv(uLightSpace, 1, GL_FALSE, &lightSpace[0][0]);
    glUniform3fv(uViewPos, 1, &camPos[0]);
    glUniform1i(uPointShadow, 4);
    glUniform1i(uHasPointShadow, hasPointLight ? 1 : 0);
    glUniform3fv(uPointLightPos, 1, &pointLightPos[0]);
    glUniform1f(uPointFar, pointShadow.farPlane());
    glUniform1i(uCSM, 5);
    glUniform1i(uEnv, 6);
    glUniformMatrix4fv(uCSMMat, CascadedShadow::N, GL_FALSE, &csm.matrices()[0][0][0]);
    glUniform1fv(uCSMSplit, CascadedShadow::N, csm.splitDepths());
    uploadLights();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadow.texture());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadow.texture());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, csm.texture());
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.texture());
    glActiveTexture(GL_TEXTURE0); // node textures bind to unit 0

    tools::DrawUniforms u;
    u.mvp = uMVP; u.model = uModel; u.color = uColor;
    u.shininess = uShininess; u.specular = uSpecular; u.alpha = uAlpha;
    u.hasNormalMap = uHasNormalMap; u.parallax = uParallax;
    u.isTerrain = uIsTerrain; u.rockColor = uRockColor;
    u.metallic = uMetallic; u.roughness = uRoughness;
    u.isWater = uIsWater;
    glUniform1f(uTime, (float)animTime);

    tools::Frustum frustum;
    frustum.fromMatrix(viewProj);

    // Parallel frustum culling: fill per-item visibility across the job pool.
    visible.assign(items.size(), 0);
    jobs.parallelFor((int)items.size(), 32, [&](int b, int e) {
        for (int i = b; i < e; ++i)
        {
            glm::vec3 c(items[i].global[3]);
            visible[i] = frustum.sphereInside(c, tools::worldRadius(items[i].global)) ? 1 : 0;
        }
    });

    if (doOcclusion)
    {
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
    }

    // Opaque pass (parallel-culled visibility + occlusion). Skipped when deferred owns opaque.
    if (drawOpaque)
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!visible[i]) continue;
        const tools::DrawItem& it = items[i];
        if (doOcclusion)
        {
            glm::vec3 c(it.global[3]);
            float r = tools::worldRadius(it.global);
            // Never trust an occluded result when the camera is inside/near the
            // query box: the box renders no fragments there and reads as occluded.
            bool nearCam = glm::length(camPos - c) < r * 1.8f + k::PerspectiveNear * 2.0f;
            auto ov = occVisible.find(it.node);
            if (!nearCam && ov != occVisible.end() && ov->second == 0) { lastOccluded++; continue; }
        }
        if (it.node->material.alpha >= 1.0f)
        {
            glUniform1i(uSelected, it.node == selectedNode ? 1 : 0);
            tools::drawItem(it, u, viewProj);
            if (doOcclusion) lastVisible++;
        }
    }

    // ECS swarm (forward-lit via the same program/uniforms).
    ecs.render(u, uSelected, viewProj);

    if (sandbox)
    {
        instances.draw(viewProj, instanceTex, glm::normalize(dirToLight),
                       lightSpace, shadow.texture()); // cast + receive shadows
        skinned.draw(viewProj, dirToLight); // procedural skinned model
        rigged.draw(viewProj, dirToLight);  // imported rigged glTF
    }

    if (doOcclusion)
    {
        // Occlusion pass: bounding-box depth-only queries (results read next frame).
        // Cull is disabled so the box is watertight from any angle.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glUseProgram(occProg);
        glBindVertexArray(cube->vao);
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (!visible[i]) continue;
            const tools::DrawItem& it = items[i];
            glm::vec3 c(it.global[3]);
            float r = tools::worldRadius(it.global);
            // Camera inside/near the box => it can't produce fragments; force it
            // visible and skip the query so a false "occluded" never sticks.
            if (glm::length(camPos - c) < r * 1.8f + k::PerspectiveNear * 2.0f)
            {
                occVisible[it.node] = 1;
                continue;
            }
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
        glEnable(GL_CULL_FACE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
    }

    if (!game || game->drawSkybox()) skybox.draw(view, proj); // fills the background

    // Transparent pass: back-to-front, blended, no depth write.
    std::vector<const tools::DrawItem*> trans;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (!visible[i]) continue;
        if (items[i].node->material.alpha < 1.0f)
        {
            trans.push_back(&items[i]);
            if (doOcclusion) lastVisible++;
        }
    }
    if (!trans.empty())
    {
        std::sort(trans.begin(), trans.end(), [&](const tools::DrawItem* a, const tools::DrawItem* b) {
            float da = glm::length(camPos - glm::vec3(a->global[3]));
            float db = glm::length(camPos - glm::vec3(b->global[3]));
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

    decals.draw(viewProj); // ground splats
    if (gpuFxOn) gpuParticles.draw(viewProj); // compute-shader particles

    // Additive particle emitters (billboarded to this view).
    glm::vec3 camRight(view[0][0], view[1][0], view[2][0]);
    glm::vec3 camUp(view[0][1], view[1][1], view[2][1]);
    for (ParticleSystem& p : emitters) p.draw(viewProj, camRight, camUp);

    // Debug overlay: collider AABBs (same box physics/broadphase test). Drawn
    // x-ray (depth test off) so colliders hidden behind geometry still show.
    if (showColliders)
    {
        // A mode with an oriented/custom collider draws its own; otherwise fall
        // back to a generic axis-aligned box per solid node.
        if (game && game->drawsColliders())
        {
            game->debugColliders(*this, debugLines);
        }
        else
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (i < visible.size() && !visible[i]) continue;
                const Node* nd = items[i].node;
                if (!nd->mesh || nd->material.alpha < 1.0f) continue; // non-solid => not a collider
                if (nd->Name == "ground" || nd->Name == "terrain") continue; // heightfield, not a box
                const glm::mat4& g = items[i].global;
                glm::vec3 half(0.5f * glm::length(glm::vec3(g[0])),
                               0.5f * glm::length(glm::vec3(g[1])),
                               0.5f * glm::length(glm::vec3(g[2])));
                debugLines.box(AABB::fromCenter(glm::vec3(g[3]), half), glm::vec3(0.15f, 1.0f, 0.35f));
            }
        }
        glDisable(GL_DEPTH_TEST);
        debugLines.flush(viewProj);
        glEnable(GL_DEPTH_TEST);
    }
}

void Engine::draw(GLFWwindow * window)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;

    prof.beginFrame();
    auto gpuT0 = std::chrono::steady_clock::now();

    // Directional light drives the shadow maps.
    glm::vec3 dirToLight(0.4f, 1.0f, 0.6f);
    for (const Light& l : lights) { if (l.type == 0) { dirToLight = l.position; break; } }
    glm::mat4 lightSpace = shadow.lightSpace(dirToLight);

    // Pass 1: single shadow map (feeds the instanced ring).
    shadow.begin();
    tools::DrawSceneDepth(scene, shadow.uLightMVP, lightSpace);
    if (sandbox) instances.drawDepth(lightSpace); // instanced ring casts shadows too
    shadow.end(width, height);

    // Pass 1a: cascaded shadow maps (scene lit pass samples these).
    csm.update(camera.view(), camera.fov, aspect, dirToLight);
    csm.render(scene, width, height);

    // Pass 1b: omnidirectional shadow cube for the first point light.
    hasPointLight = false;
    for (const Light& l : lights) { if (l.type == 1) { pointLightPos = l.position; hasPointLight = true; break; } }
    if (hasPointLight) pointShadow.render(scene, pointLightPos, width, height);
    prof.mark("shadows");

    updateLOD(scene.MainNode, glm::mat4(1.0f)); // distance-based mesh swap
    std::vector<tools::DrawItem> items;
    tools::collect(scene.MainNode, glm::mat4(1.0f), items);

    if (splitScreen)
    {
        // Two viewports side by side; second uses the top-down preset. No post-fx.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float aspect2 = height > 0 ? (float)(width / 2) / (float)height : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect2, k::PerspectiveNear, k::PerspectiveFar);
        const Camera& cam2 = camPresets.size() > 1 ? camPresets[1] : camera;

        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, width / 2, height);
        glScissor(0, 0, width / 2, height);
        renderForward(camera.view(), proj, camera.position, dirToLight, lightSpace, items, false);

        glViewport(width / 2, 0, width - width / 2, height);
        glScissor(width / 2, 0, width - width / 2, height);
        renderForward(cam2.view(), proj, cam2.position, dirToLight, lightSpace, items, false);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);

        lastTotal = (int)items.size();
    }
    else
    {
        glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect, k::PerspectiveNear, k::PerspectiveFar);
        glm::mat4 viewProj = proj * camera.view();
        glm::mat4 invVP = glm::inverse(viewProj);

        // Offscreen targets for post-processing (lazy init + resize).
        if (postW != width || postH != height)
        {
            if (postW == 0) { post.init(width, height); deferred.init(width, height); }
            else { post.resize(width, height); deferred.resize(width, height); }
            postW = width; postH = height;
        }

        if (deferredMode)
        {
            // G-buffer geometry pass: opaque scene nodes only.
            tools::DrawUniforms gu = deferred.beginGeometry();
            tools::Frustum fr; fr.fromMatrix(viewProj);
            for (const tools::DrawItem& it : items)
            {
                glm::vec3 c(it.global[3]);
                if (it.node->material.alpha >= 1.0f &&
                    fr.sphereInside(c, tools::worldRadius(it.global)))
                    tools::drawItem(it, gu, viewProj);
            }
            // Deferred lighting into the HDR scene buffer.
            post.bind();
            glClearColor(0.5f, 0.2f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            deferred.lighting(invVP, camera.position, lights, csm, pointShadow, hasPointLight, pointLightPos, skybox.texture());
            glEnable(GL_DEPTH_TEST);
            // Bring G-buffer depth over so the forward overlay depth-tests correctly.
            deferred.blitDepthTo(post.sceneFramebuffer());
            post.bind();
            // Forward overlay: transparency + instances + skybox + particles (no opaque).
            renderForward(camera.view(), proj, camera.position, dirToLight, lightSpace, items, false, false);
        }
        else
        {
            post.bind();
            if (game && game->clearBlack()) glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // e.g. space
            else           glClearColor(0.5f, 0.2f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderForward(camera.view(), proj, camera.position, dirToLight, lightSpace, items, true);
        }

        // Depth-of-field focus: distance to the selected node, else a default.
        float focusDist = 6.0f;
        if (!selectedName.empty())
        {
            glm::vec3 sp;
            if (worldPosOf(scene.MainNode, glm::mat4(1.0f), selectedName, sp))
                focusDist = glm::length(camera.position - sp);
        }

        // Sun screen position for volumetric light shafts.
        glm::vec3 sunColor(1.0f);
        for (const Light& l : lights) { if (l.type == 0) { sunColor = l.color; break; } }
        glm::vec3 sunWorld = camera.position + glm::normalize(dirToLight) * 50.0f;
        glm::vec4 sunClip = viewProj * glm::vec4(sunWorld, 1.0f);
        glm::vec2 sunUV(0.0f);
        float sunVisible = 0.0f;
        if (sunClip.w > 0.0f)
        {
            sunUV = glm::vec2(sunClip) / sunClip.w * 0.5f + 0.5f;
            if (sunUV.x >= 0.0f && sunUV.x <= 1.0f && sunUV.y >= 0.0f && sunUV.y <= 1.0f)
                sunVisible = 1.0f;
        }

        // Resolve offscreen scene to the screen with post-processing
        // (SSAO / SSR / shafts / DoF / fog / bloom / tonemap / LUT / grain).
        post.draw(width, height, k::PerspectiveNear, k::PerspectiveFar,
                  (float)animTime, invVP, prevViewProj, viewProj, camera.position,
                  focusDist, sunUV, sunVisible, sunColor,
                  game ? game->cinematic() : 1.0f); // modes may disable motion blur / DoF
        prevViewProj = viewProj;
    }
    prof.mark("scene+post");

    // HUD overlay: 2D text, no depth, no cull, alpha-blended.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    text.draw("smallgine  FPS " + std::to_string(currentFps) +
              "  visible " + std::to_string(lastVisible) + "/" + std::to_string(lastTotal) +
              "  occluded " + std::to_string(lastOccluded),
              12.0f, 26.0f, width, height, glm::vec3(1.0f, 1.0f, 1.0f));
    if (game)
    {
        game->hud(*this); // demo draws its own HUD
    }
    else
    {
        // Editor readout: selected node name + live transform.
        if (!selectedName.empty())
        {
            if (Node* sn = scene.MainNode.find(selectedName))
            {
                char buf[192];
                std::snprintf(buf, sizeof(buf),
                    "sel '%s'  pos %.2f %.2f %.2f  rot %.0f %.0f %.0f  scl %.2f %.2f %.2f",
                    selectedName.c_str(), sn->Position.x, sn->Position.y, sn->Position.z,
                    sn->Rotation.x, sn->Rotation.y, sn->Rotation.z,
                    sn->Scale.x, sn->Scale.y, sn->Scale.z);
                text.draw(buf, 12.0f, 48.0f, width, height, glm::vec3(0.7f, 1.0f, 0.8f));
            }
        }
        text.draw("+", width * 0.5f - 5.0f, height * 0.5f + 6.0f, width, height, glm::vec3(1.0f)); // crosshair
        text.draw("WASD fly | F pick | I editor | P drop | K nav | V split | O SSAO | L SSR | G defer | T reverb | C cam | F5/F9",
                  12.0f, (float)height - 14.0f, width, height, glm::vec3(0.9f, 0.9f, 0.6f));
    }

    // Editor panel: colored button rects (UI shader) + text labels.
    if (editorOpen)
    {
        std::vector<UIButton> btns = editorButtons(width, height);
        Rect bg{(float)width - 216.0f, 82.0f, 212.0f, (float)btns.size() * 30.0f + 8.0f};
        ui.rect(bg, glm::vec4(0.06f, 0.07f, 0.10f, 0.88f), width, height);
        for (const UIButton& b : btns)
        {
            bool hover = b.rect.contains((float)lastX, (float)lastY);
            ui.rect(b.rect, hover ? glm::vec4(0.24f, 0.36f, 0.52f, 0.95f)
                                  : glm::vec4(0.15f, 0.18f, 0.24f, 0.95f), width, height);
            text.draw(b.label, b.rect.x + 8.0f, b.rect.y + 18.0f, width, height, glm::vec3(0.9f, 0.95f, 1.0f));
        }
        text.draw("EDITOR  (I to close)", (float)width - 210.0f, 74.0f, width, height, glm::vec3(1.0f, 0.9f, 0.5f));
    }

    // Profiler overlay: per-section CPU ms + frame/GPU totals.
    if (prof.on())
    {
        float y = 92.0f;
        char line[96];
        for (const auto& s : prof.report())
        {
            std::snprintf(line, sizeof(line), "%-12s %6.2f ms", s.first.c_str(), s.second);
            text.draw(line, 12.0f, y, width, height, glm::vec3(0.6f, 1.0f, 0.9f));
            y += 22.0f;
        }
        std::snprintf(line, sizeof(line), "frame %6.2f ms  gpu %6.2f ms", prof.totalMs(), prof.gpu());
        text.draw(line, 12.0f, y, width, height, glm::vec3(1.0f, 0.95f, 0.6f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    prof.mark("hud");
    if (prof.on())
    {
        glFinish(); // stall to measure GPU completion (debug overlay only)
        prof.setGpuMs(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - gpuT0).count());
    }
    prof.endFrame();

    if (captureReq) { captureScreenshot(width, height); captureReq = false; }

    tools::glCheck("draw");
}

void Engine::process(double dt)
{
    if (game)
    {
        game->update(*this, (float)dt); // demo controls the camera + gameplay
    }
    else
    {
        // Default free-fly camera (engine sandbox / editor).
        float v = camera.speed * (float)dt;
        glm::vec3 right = camera.right();
        if (keyW) camera.position += camera.front * v;
        if (keyS) camera.position -= camera.front * v;
        if (keyA) camera.position -= right * v;
        if (keyD) camera.position += right * v;
        if (keyQ) camera.position += camera.up * v;
        if (keyE) camera.position -= camera.up * v;
    }

    // Per-node behavior: keyframe animation + data-driven spin.
    animTime += dt;
    updateNode(scene.MainNode, (float)dt, (float)animTime);

    // Particle emitters + skinned model + rigid-body physics.
    for (ParticleSystem& p : emitters) p.update((float)dt);
    skinned.update((float)animTime);
    rigged.update((float)animTime);
    stepPhysics((float)dt);
    stepAgent((float)dt);
    updateTriggers();

    // ECS: movement system parallelized across the job pool, spin serial.
    jobs.parallelFor(ecs.count(), 8, [&](int b, int e) { ecs.move(b, e, (float)dt); });
    ecs.spinSystem((float)dt);

    // GPU particle sim (compute shader integrates state in its SSBO).
    if (gpuFxOn) gpuParticles.update((float)dt);

    // Lua behaviors: update(dt, t) drives scripted nodes.
    script.update((float)dt, (float)animTime);

    // Networking: server orbits the source + sends it; client applies to the ghost.
    if (netOn)
    {
        if (Node* src = scene.MainNode.find("netsrc"))
        {
            float t = (float)animTime;
            src->Position = glm::vec3(std::cos(t) * 2.2f, 2.6f, std::sin(t) * 2.2f);
            net.send(1, src->Position);
        }
        uint32_t id; glm::vec3 p;
        while (net.poll(id, p))
            if (Node* gh = scene.MainNode.find("netghost")) gh->Position = p + glm::vec3(0.0f, 0.7f, 0.0f);
    }

    // Shader hot-reload: poll lit shader mtimes a few times per second.
    shaderPoll += dt;
    if (shaderPoll >= 0.4)
    {
        shaderPoll = 0.0;
        long vm = tools::fileMtime(resolvePath("assets/shaders/lit.vert"));
        long fm = tools::fileMtime(resolvePath("assets/shaders/lit.frag"));
        if ((vm && vm != litVertMtime) || (fm && fm != litFragMtime))
        {
            litVertMtime = vm; litFragMtime = fm;
            reloadShaders();
        }
        watchAssets(); // textures + scene file
    }

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

} // namespace smallgine
