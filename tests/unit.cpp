// No-GL unit tests: data structures, JSON round-trips, and transform math.
// Builds without a window/context (GL headers included but no GL calls made).
#include "core/scene.hpp"
#include "tools/noise.hpp"
#include "tools/packager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <unistd.h>

using namespace smallgine;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cout << "FAIL: " #cond " (line " << __LINE__ << ")\n"; ++failures; } } while (0)
static bool nearly(float a, float b) { return std::fabs(a - b) < 1e-4f; }
static bool vnear(const glm::vec3& a, const glm::vec3& b) { return nearly(a.x, b.x) && nearly(a.y, b.y) && nearly(a.z, b.z); }

static void testMaterialRoundTrip()
{
    Material m;
    m.color = {0.2f, 0.4f, 0.6f};
    m.texture = "assets/x.tga";
    m.shininess = 48.0f; m.specular = 0.7f; m.alpha = 0.5f; m.parallax = 0.03f;
    m.metallic = 1.0f; m.roughness = 0.25f; m.terrain = true; m.rockColor = {0.1f, 0.2f, 0.3f};
    m.water = true;
    Material r = nlohmann::json(m).get<Material>();
    CHECK(r.water == true);
    CHECK(vnear(r.color, m.color));
    CHECK(r.texture == m.texture);
    CHECK(nearly(r.metallic, 1.0f));
    CHECK(nearly(r.roughness, 0.25f));
    CHECK(r.terrain == true);
    CHECK(vnear(r.rockColor, m.rockColor));
    CHECK(nearly(r.alpha, 0.5f));
}

static void testNodeRoundTrip()
{
    Node n;
    n.Name = "root";
    n.Position = {1.0f, 2.0f, 3.0f};
    n.Rotation = {10.0f, 20.0f, 30.0f};
    n.Scale = {0.5f, 0.5f, 0.5f};
    n.Dynamic = true;
    n.Emitter.enabled = true; n.Emitter.count = 200; n.Emitter.size = 0.2f;
    Node child; child.Name = "c"; child.Position = {0.0f, 1.0f, 0.0f};
    n.addChild(child);

    Node r = nlohmann::json(n).get<Node>();
    CHECK(r.Name == "root");
    CHECK(vnear(r.Position, n.Position));
    CHECK(vnear(r.Rotation, n.Rotation));
    CHECK(r.Dynamic == true);
    CHECK(r.Children.size() == 1);
    CHECK(r.Children[0].Name == "c");
    CHECK(r.Emitter.enabled == true);
    CHECK(r.Emitter.count == 200);
}

static void testSceneRoundTrip()
{
    Scene s;
    s.name = "demo";
    s.MainNode.Name = "root";
    s.Lights.push_back({0, glm::vec3(0.4f, 1.0f, 0.6f), glm::vec3(1.0f), 0.7f});
    Scene r = nlohmann::json(s).get<Scene>();
    CHECK(r.name == "demo");
    CHECK(r.MainNode.Name == "root");
    CHECK(r.Lights.size() == 1);
    CHECK(nearly(r.Lights[0].intensity, 0.7f));
}

static void testLocalMatrix()
{
    Node n;
    n.Position = {2.0f, -1.0f, 4.0f};
    glm::mat4 m = n.localMatrix();
    CHECK(vnear(glm::vec3(m[3]), n.Position)); // translation column
}

static void testAnimation()
{
    Node n;
    Keyframe a; a.t = 0.0f; a.position = {0.0f, 0.0f, 0.0f};
    Keyframe b; b.t = 2.0f; b.position = {0.0f, 4.0f, 0.0f};
    n.Animation = {a, b};
    n.evalAnimation(1.0f); // midpoint
    CHECK(vnear(n.Position, glm::vec3(0.0f, 2.0f, 0.0f)));
}

static void testNoiseRange()
{
    // Every generator must stay in [0,1] over a spread of samples, and be finite.
    for (int ti = 0; ti < (int)noise::Type::Count; ++ti)
    {
        noise::Type t = (noise::Type)ti;
        for (int i = 0; i < 200; ++i)
        {
            float x = (float)i * 0.137f - 7.0f;
            float y = (float)i * 0.291f + 3.0f;
            float v = noise::sample01(t, x, y, 42);
            CHECK(std::isfinite(v));
            CHECK(v >= 0.0f && v <= 1.0f);
        }
    }
}

static void testNoiseDeterministic()
{
    // Same seed => identical output; different seed => generally different.
    CHECK(nearly(noise::perlin2(1.5f, 2.5f, 7), noise::perlin2(1.5f, 2.5f, 7)));
    CHECK(nearly(noise::simplex2(0.3f, 9.1f, 1), noise::simplex2(0.3f, 9.1f, 1)));
    CHECK(!nearly(noise::perlin2(1.5f, 2.5f, 7), noise::perlin2(1.5f, 2.5f, 8)));
}

static void testNoiseContinuity()
{
    // Gradient/value noise are continuous: a tiny step yields a tiny change.
    float a = noise::perlin2(3.2f, 1.1f, 5);
    float b = noise::perlin2(3.2f + 1e-3f, 1.1f, 5);
    CHECK(std::fabs(a - b) < 0.05f);
    // Lattice points evaluate to ~0 for Perlin gradient noise.
    CHECK(std::fabs(noise::perlin2(4.0f, 6.0f, 5)) < 1e-4f);
}

static void testNoiseFractal()
{
    // fBm of a zero-mean base stays roughly zero-mean and bounded.
    auto base = [](float x, float y) { return noise::perlin2(x, y, 11); };
    noise::Fractal fp; fp.octaves = 6;
    for (int i = 0; i < 50; ++i)
    {
        float v = noise::fbm(base, (float)i * 0.3f, (float)i * 0.7f, fp);
        CHECK(v >= -1.001f && v <= 1.001f);
    }
    // Ridged output is normalized to [0,1].
    float r = noise::ridged(base, 2.3f, 4.5f, fp);
    CHECK(r >= 0.0f && r <= 1.0f);
}

static void testPackRoundTrip()
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / ("sgpk_test_" + std::to_string(::getpid()));
    fs::path src = root / "assets";
    fs::create_directories(src / "shaders");
    auto put = [](const fs::path& p, const std::string& s) {
        std::ofstream f(p, std::ios::binary); f.write(s.data(), (std::streamsize)s.size());
    };
    std::string txt = "hello pack";
    std::string bin(std::string("\x00\x01\x02\xff" "bytes", 9)); // embedded NUL + high byte
    put(src / "hello.txt", txt);
    put(src / "shaders" / "x.glsl", bin);

    fs::path out = root / "assets.sgpk";
    int n = smallgine::packDirectory(src.string(), out.string(), "assets");
    CHECK(n == 2);

    CHECK(smallgine::mountPack(out.string()));
    CHECK(smallgine::packMounted());
    CHECK(smallgine::readAssetText("assets/hello.txt") == txt);        // key = prefix + rel
    std::vector<unsigned char> got;
    CHECK(smallgine::readAsset("assets/shaders/x.glsl", got));         // nested + binary-safe
    CHECK(std::string(got.begin(), got.end()) == bin);
    CHECK(smallgine::readAsset("assets/shaders\\x.glsl", got));        // backslash normalizes
    CHECK(!smallgine::assetExists("assets/nope.txt"));                 // absent, no loose fallback

    fs::remove_all(root);
}

int main()
{
    testMaterialRoundTrip();
    testPackRoundTrip();
    testNodeRoundTrip();
    testSceneRoundTrip();
    testLocalMatrix();
    testAnimation();
    testNoiseRange();
    testNoiseDeterministic();
    testNoiseContinuity();
    testNoiseFractal();

    if (failures == 0) { std::cout << "unit tests: ALL PASS\n"; return 0; }
    std::cout << "unit tests: " << failures << " FAILED\n";
    return 1;
}
