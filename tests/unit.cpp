// No-GL unit tests: data structures, JSON round-trips, and transform math.
// Builds without a window/context (GL headers included but no GL calls made).
#include "core/scene.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>

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
    Material r = nlohmann::json(m).get<Material>();
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

int main()
{
    testMaterialRoundTrip();
    testNodeRoundTrip();
    testSceneRoundTrip();
    testLocalMatrix();
    testAnimation();

    if (failures == 0) { std::cout << "unit tests: ALL PASS\n"; return 0; }
    std::cout << "unit tests: " << failures << " FAILED\n";
    return 1;
}
