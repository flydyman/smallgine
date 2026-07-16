#pragma once
#include "core/engine.hpp"
#include <glm/gtc/quaternion.hpp>
#include <cstdlib>

namespace smallgine {

// Space flight demo: 6DOF quaternion ship + Newtonian momentum, chase camera,
// asteroid field. Background is black (no skybox, no motion blur / DoF).
class SpaceMode : public IGameMode
{
    glm::quat orient{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 pos{0.0f}, vel{0.0f};
    bool fly = false;      // false = drift (Newtonian), true = fly (arcade)
    float flySpeed = 0.0f;
    std::shared_ptr<Mesh> rockMesh;
    Texture rockTex;

public:
    void setup(Engine& e) override
    {
        Scene& scene = e.gameScene();
        e.gameCaptureMouse(false);
        scene.MainNode.mesh = nullptr; // empty root would otherwise draw a stray cube
        rockMesh = makeAsteroid(22, 30, 0.42f, 6.0f);
        rockTex = makeRockTexture(256);
        GLuint stex = rockTex.id;
        auto rnd = [](float a, float b) { return a + (b - a) * (float)(std::rand() % 1000) / 1000.0f; };

        {
            Node s; s.Name = "ship"; s.Scale = glm::vec3(0.7f, 0.55f, 1.6f);
            s.material.color = glm::vec3(0.75f, 0.78f, 0.85f); s.material.texture = "assets/test.tga";
            s.material.metallic = 0.7f; s.material.roughness = 0.3f; s.materialSet = true;
            s.mesh = e.gameCube(); s.texId = e.gameTexture("assets/test.tga");
            s.useRotMatrix = true; s.RotMatrix = glm::mat4(1.0f);
            scene.MainNode.Children.push_back(s);
        }
        for (int i = 0; i < 70; ++i)
        {
            glm::vec3 p;
            do { p = glm::vec3(rnd(-60.0f, 60.0f), rnd(-35.0f, 35.0f), rnd(-90.0f, 20.0f)); }
            while (glm::length(p) < 12.0f);
            Node r; r.Name = "rock" + std::to_string(i); r.Position = p;
            float sc = rnd(1.2f, 5.0f);
            r.Scale = glm::vec3(sc * rnd(0.7f, 1.3f), sc * rnd(0.7f, 1.3f), sc * rnd(0.7f, 1.3f));
            r.Rotation = glm::vec3(rnd(0.0f, 360.0f), rnd(0.0f, 360.0f), rnd(0.0f, 360.0f));
            r.Spin = glm::vec3(rnd(-15.0f, 15.0f), rnd(-15.0f, 15.0f), 0.0f);
            float g = rnd(0.5f, 0.75f);
            r.material.color = glm::vec3(g, g * 0.97f, g * 0.92f);
            r.material.roughness = 0.95f; r.materialSet = true;
            r.mesh = rockMesh; r.texId = stex;
            scene.MainNode.Children.push_back(r);
        }

        pos = glm::vec3(0.0f); vel = glm::vec3(0.0f);
        Camera& cam = e.gameCamera();
        cam.position = pos + glm::vec3(0.0f, 3.0f, 9.0f);
        cam.front = glm::normalize(pos - cam.position);
        cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
        std::cout << "Space: ship + 70 asteroids" << std::endl;
    }

    void onKeyPress(Engine& e, int key) override
    {
        if (key != GLFW_KEY_X) return;
        fly = !fly;
        if (fly) flySpeed = glm::dot(vel, orient * glm::vec3(0, 0, -1));
        std::cout << "Flight mode: " << (fly ? "fly (arcade)" : "drift (newtonian)") << std::endl;
    }

    void update(Engine& e, float dt) override
    {
        const float turn = glm::radians(70.0f) * dt;
        const float thrust = 14.0f, brake = 1.4f, maxSpeed = 40.0f;

        float pitch = (e.kS() ? 1.0f : 0.0f) - (e.kW() ? 1.0f : 0.0f);
        float yaw   = (e.kA() ? 1.0f : 0.0f) - (e.kD() ? 1.0f : 0.0f);
        float roll  = (e.kQ() ? 1.0f : 0.0f) - (e.kE() ? 1.0f : 0.0f);
        glm::quat dq(glm::vec3(pitch * turn, yaw * turn, roll * turn));
        orient = glm::normalize(orient * dq);

        glm::vec3 fwd = orient * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up  = orient * glm::vec3(0.0f, 1.0f, 0.0f);

        if (fly)
        {
            if (e.kShift()) flySpeed += thrust * dt;
            if (e.kCtrl())  flySpeed -= thrust * dt;
            flySpeed = std::max(0.0f, std::min(maxSpeed, flySpeed));
            vel = fwd * flySpeed;
        }
        else
        {
            if (e.kShift()) vel += fwd * thrust * dt;
            if (e.kCtrl())  vel -= vel * std::min(1.0f, brake * dt);
            float sp = glm::length(vel);
            if (sp > maxSpeed) vel *= maxSpeed / sp;
        }
        pos += vel * dt;

        if (Node* s = e.gameScene().MainNode.find("ship"))
        {
            s->Position = pos; s->useRotMatrix = true; s->RotMatrix = glm::mat4_cast(orient);
        }

        Camera& cam = e.gameCamera();
        glm::vec3 want = pos - fwd * 9.0f + up * 3.0f;
        float k = std::min(1.0f, 5.0f * dt);
        cam.position += (want - cam.position) * k;
        cam.front = glm::normalize((pos + fwd * 6.0f) - cam.position);
        cam.up = up;
    }

    void hud(Engine& e) override
    {
        int w = e.gameScreenW(), h = e.gameScreenH();
        TextRenderer& t = e.gameText();
        t.draw("[ ]", w * 0.5f - 12.0f, h * 0.5f + 6.0f, w, h, glm::vec3(0.5f, 1.0f, 0.7f));
        char line[96];
        std::snprintf(line, sizeof(line), "SPEED %5.1f m/s", glm::length(vel));
        t.draw(line, w - 240.0f, 30.0f, w, h, glm::vec3(0.5f, 1.0f, 0.7f));
        std::string state = e.kShift() ? "THRUST" : (e.kCtrl() ? "BRAKE" : "COAST");
        t.draw(std::string(fly ? "FLY" : "DRIFT") + "   " + state, 12.0f, 50.0f, w, h, glm::vec3(0.8f, 0.95f, 1.0f));
        t.draw("Shift thrust | Ctrl brake | W/S pitch | A/D yaw | Q/E roll | X mode | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }

    void teardown(Engine&) override
    {
        if (rockMesh) rockMesh->free();
        if (rockTex.id) glDeleteTextures(1, &rockTex.id);
    }

    bool drawSkybox() const override { return false; }
    bool clearBlack() const override { return true; }
    float cinematic() const override { return 0.0f; }
};

} // namespace smallgine
