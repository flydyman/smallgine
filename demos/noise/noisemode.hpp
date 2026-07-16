#pragma once
#include "core/engine.hpp"
#include "tools/noise.hpp"
#include <cstdio>

namespace smallgine {

// Procedural-noise showcase. Builds a wide terrain grid whose heights come from
// one of the library's generators (tools/noise.hpp) and paints it as a landscape
// — sand along the shore, grass and forest on the flats, bare rock on cliffs, and
// snow on the peaks — by sampling a (elevation, steepness) palette. Cycle the
// generators live to compare their character. Free-fly camera (WASD + mouse),
// N/B to switch generator, R to reseed, [ / ] to zoom the noise domain, P to
// toggle palette painting vs a flat slope tint.
class NoiseMode : public IGameMode
{
    static constexpr int   kGrid   = 220;    // terrain resolution
    static constexpr float kWorld  = 44.0f;  // terrain footprint (world units)
    static constexpr float kHeight = 7.0f;   // vertical world scale (peak height)
    static constexpr float kRelief = 1.0f;   // local height amplitude fed to the mesh
    static constexpr float kSeaBase = -2.0f; // terrain node base Y (elevation 0)
    static constexpr float kSeaLvl = 0.18f;  // sea level as a normalized elevation

    noise::Type type = noise::Type::Fbm;
    uint32_t    seed = 1337;
    float       domain = 5.0f;               // noise features across the tile
    bool        paint = true;                // palette painting vs slope tint
    bool        water = true;                // show the sea-level water plane
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Mesh> waterMesh;
    Texture     palette;

    void rebuild(Engine& e)
    {
        // Coarse scan of the field's range so the palette spans shore -> peak for
        // every generator (raw fBm/simplex sit mid-range and would look all-grass).
        const int scan = 64;
        float lo = 1e9f, hi = -1e9f;
        for (int j = 0; j <= scan; ++j)
            for (int i = 0; i <= scan; ++i)
            {
                float s = noise::sample01(type, (float)i / scan * domain,
                                          (float)j / scan * domain, seed);
                lo = std::min(lo, s); hi = std::max(hi, s);
            }
        float inv = (hi > lo) ? 1.0f / (hi - lo) : 1.0f;
        const float contrast = 1.25f; // widen the tails so sand & snow show up
        auto sampler = [=](float u, float v) {
            float s = noise::sample01(type, u * domain, v * domain, seed);
            float n = (s - lo) * inv;
            n = (n - 0.5f) * contrast + 0.5f;
            return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        };
        // Palette UVs encode (elevation, world-space steepness); aspect = y/xz scale.
        std::shared_ptr<Mesh> next = makeHeightMesh(kGrid, sampler, kRelief, nullptr,
                                                    paint, kHeight / kWorld);
        if (Node* t = e.gameScene().MainNode.find("noise"))
        {
            t->mesh = next;
            t->texId = paint ? palette.id : e.gameTexture("assets/test.tga");
            t->material.terrain = !paint; // slope-tint path when not painting
            t->material.color = paint ? glm::vec3(1.0f) : glm::vec3(0.34f, 0.55f, 0.30f);
        }
        if (mesh) mesh->free();
        mesh = next;
        std::printf("Noise: %-22s  seed=%u  domain=%.1f  paint=%s\n",
                    noise::name(type), seed, domain, paint ? "on" : "off");
    }

public:
    void setup(Engine& e) override
    {
        Scene& scene = e.gameScene();
        scene.MainNode.mesh = nullptr; // don't draw the empty root as a cube
        palette = makeTerrainPalette(128);
        waterMesh = makeWaterPlane(8);

        Node t;
        t.Name = "noise";
        t.Position = glm::vec3(0.0f, kSeaBase, 0.0f);
        t.Scale = glm::vec3(kWorld, kHeight, kWorld);
        t.material.color = glm::vec3(1.0f);
        t.material.rockColor = glm::vec3(0.40f, 0.36f, 0.32f); // rock (steep, slope-tint mode)
        t.material.roughness = 0.92f;
        t.material.texture = "assets/test.tga";
        t.materialSet = true;
        scene.MainNode.Children.push_back(t);

        // Sea-level water plane: translucent, reflective, animated (uIsWater path).
        Node w;
        w.Name = "water";
        w.Position = glm::vec3(0.0f, kSeaBase + kSeaLvl * kHeight, 0.0f);
        w.Scale = glm::vec3(kWorld * 1.02f, 1.0f, kWorld * 1.02f);
        w.material.color = glm::vec3(0.10f, 0.30f, 0.42f);
        w.material.water = true;
        w.material.alpha = 0.74f;
        w.material.roughness = 0.06f;
        w.material.specular = 0.9f;
        w.material.metallic = 0.0f;
        w.material.texture = "assets/test.tga"; // opaque-alpha albedo (color drives tint)
        w.materialSet = true;
        w.mesh = waterMesh;
        w.texId = e.gameTexture("assets/test.tga");
        scene.MainNode.Children.push_back(w);

        rebuild(e);

        // Low, close vantage: foreground stays crisp while distant peaks fade into
        // the engine's distance fog (atmospheric perspective) rather than the whole
        // wide landscape washing out.
        Camera& cam = e.gameCamera();
        cam.position = glm::vec3(0.0f, 11.0f, 21.0f);
        cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
        cam.yaw = -90.0f;   // face -Z
        cam.pitch = -22.0f; // look down across the water and hills
        cam.updateVectors();
        std::cout << "Noise demo: N/B generator, R reseed, [ / ] zoom, P paint, X water\n";
    }

    void onKeyPress(Engine& e, int key) override
    {
        int n = (int)noise::Type::Count;
        switch (key)
        {
            case GLFW_KEY_N: type = (noise::Type)(((int)type + 1) % n);           rebuild(e); break;
            case GLFW_KEY_B: type = (noise::Type)(((int)type + n - 1) % n);       rebuild(e); break;
            case GLFW_KEY_R: seed = noise::hashU(seed + 0x9e3779b9u);             rebuild(e); break;
            case GLFW_KEY_P: paint = !paint;                                      rebuild(e); break;
            case GLFW_KEY_X:
                water = !water;
                if (Node* w = e.gameScene().MainNode.find("water"))
                    w->mesh = water ? waterMesh : nullptr; // no mesh => not drawn
                std::printf("Water: %s\n", water ? "on" : "off");
                break;
            case GLFW_KEY_LEFT_BRACKET:  domain = std::max(1.0f, domain - 1.0f);  rebuild(e); break;
            case GLFW_KEY_RIGHT_BRACKET: domain = std::min(24.0f, domain + 1.0f); rebuild(e); break;
            default: break;
        }
    }

    void update(Engine& e, float dt) override
    {
        // Free-fly camera (the engine's default is suppressed once a mode drives update).
        Camera& cam = e.gameCamera();
        float v = cam.speed * dt * (e.kShift() ? 3.0f : 1.0f);
        glm::vec3 right = cam.right();
        if (e.kW()) cam.position += cam.front * v;
        if (e.kS()) cam.position -= cam.front * v;
        if (e.kA()) cam.position -= right * v;
        if (e.kD()) cam.position += right * v;
        if (e.kQ()) cam.position += cam.up * v;
        if (e.kE()) cam.position -= cam.up * v;
    }

    void hud(Engine& e) override
    {
        int w = e.gameScreenW(), h = e.gameScreenH();
        TextRenderer& t = e.gameText();
        char line[160];
        std::snprintf(line, sizeof(line), "%s   seed %u   domain %.0f   paint %s   water %s",
                      noise::name(type), seed, domain, paint ? "on" : "off", water ? "on" : "off");
        t.draw(line, 12.0f, 30.0f, w, h, glm::vec3(0.85f, 1.0f, 0.7f));
        t.draw("N/B generator | R reseed | [ / ] zoom | P paint | X water | WASD+mouse fly | Shift boost | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }

    void teardown(Engine&) override
    {
        if (mesh) mesh->free();
        if (waterMesh) waterMesh->free();
        if (palette.id) glDeleteTextures(1, &palette.id);
    }

    // Crisp landscape view: no motion-blur / depth-of-field / fog smear.
    float cinematic() const override { return 0.0f; }
};

} // namespace smallgine
