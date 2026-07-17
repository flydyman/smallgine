#pragma once
#include "core/engine.hpp"
#include "tools/helpers.hpp"
#include "tools/broadphase.hpp"
#include <cstdio>

namespace smallgine {

// FPS demo: capsule walker on the level's scene. A blaster viewmodel rides the
// camera and fires glowing bolts; the shot is a mode-side ray hitscan on target*
// (not the engine pick, which would just hit the viewmodel under the crosshair).
// The camera bobs/shakes while walking and kicks back on every shot.
class FpsMode : public IGameMode
{
    glm::vec3 playerPos{0.0f};
    float playerVelY = 0.0f;
    bool onGround = false, jumpReq = false;
    const float eyeHeight = 1.6f, playerRadius = 0.3f;
    int score = 0;

    // View feel.
    float bobPhase = 0.0f;   // walk-cycle phase (advances with movement)
    float bobAmp   = 0.0f;   // eased 0..1 by whether we're moving on the ground
    float recoil   = 0.0f;   // 1 on fire, decays; drives kickback + camera kick
    float flash    = 0.0f;   // muzzle-flash timer, decays
    float cooldown = 0.0f;   // seconds until the blaster can fire again

    // Viewmodel placement, relative to the camera basis (tune here).
    static constexpr float kReach   =  0.90f; // forward from the eye
    static constexpr float kRight   =  0.30f; // to the right of centre (off crosshair)
    static constexpr float kDown    =  0.26f; // below the eye
    static constexpr float kBarrel  =  0.85f; // muzzle ahead of the grip (bolt spawn)

    // Selectable weapons (classic 1/2 keys, or Q to cycle). The Kenney blaster
    // models are authored -Z forward, so the pose flips them 180 deg (see update).
    struct Weapon { const char* name; const char* model; float scale; float fireRate; glm::vec3 bolt; };
    static const int kWeapons = 2;
    Weapon weapons[kWeapons] = {
        { "BLASTER", "assets/blaster-b.glb", 0.8f, 0.28f, glm::vec3(0.4f, 3.0f, 1.2f) }, // green, slower
        { "RIFLE",   "assets/blaster-n.glb", 0.8f, 0.11f, glm::vec3(3.0f, 1.6f, 0.4f) }, // orange, rapid
    };
    int weapon = 1; // start on the rifle

    // Glowing bolt tracers (fixed pool, ring-buffered on fire).
    struct Bolt { glm::vec3 pos{0.0f}, vel{0.0f}; float life = 0.0f; };
    static const int kBolts = 10;
    Bolt bolts[kBolts];
    int nextBolt = 0;

    // Push the player capsule out of solid, opaque scene boxes (walls / crates).
    // Skips the viewmodel + tracers, which live on top of the camera.
    void collide(Scene& scene)
    {
        glm::vec3 half(playerRadius, eyeHeight * 0.5f, playerRadius);
        glm::vec3 center(playerPos.x, playerPos.y + eyeHeight * 0.5f, playerPos.z);
        AABB pbox = AABB::fromCenter(center, half);
        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name == "ground" || nd->Name == "terrain" || nd->material.alpha < 1.0f) continue;
            if (nd->Name == "blaster" || nd->Name == "muzzle" ||
                nd->Name.rfind("bolt", 0) == 0) continue; // viewmodel/tracers aren't world geometry
            glm::vec3 bh(0.5f * glm::length(glm::vec3(it.global[0])),
                         0.5f * glm::length(glm::vec3(it.global[1])),
                         0.5f * glm::length(glm::vec3(it.global[2])));
            glm::vec3 bc(it.global[3]);
            AABB box = AABB::fromCenter(bc, bh);
            if (!pbox.overlaps(box)) continue;
            if (bc.y + bh.y < playerPos.y + 0.05f) continue; // low enough to stand on
            float px = std::min(pbox.max.x - box.min.x, box.max.x - pbox.min.x);
            float pz = std::min(pbox.max.z - box.min.z, box.max.z - pbox.min.z);
            if (px < pz) playerPos.x += (center.x < bc.x ? -px : px);
            else         playerPos.z += (center.z < bc.z ? -pz : pz);
            center = glm::vec3(playerPos.x, playerPos.y + eyeHeight * 0.5f, playerPos.z);
            pbox = AABB::fromCenter(center, half);
        }
    }

    // Muzzle position in world space from an orthonormal camera basis. cam.up is
    // the fixed world up, so the true up is rebuilt from front x right.
    glm::vec3 muzzle(const Camera& cam) const
    {
        glm::vec3 fwd = glm::normalize(cam.front);
        glm::vec3 rt  = cam.right();
        glm::vec3 up  = glm::normalize(glm::cross(rt, fwd));
        return cam.position + fwd * (kReach + kBarrel) + rt * kRight - up * kDown;
    }

    // Swap the "blaster" node to weapon `idx` (mesh + scale + texture + tracer
    // colour). The node keeps its name/place; only its geometry changes.
    void equip(Engine& e, int idx)
    {
        weapon = (idx % kWeapons + kWeapons) % kWeapons;
        const Weapon& w = weapons[weapon];
        Node* g = e.gameScene().MainNode.find("blaster");
        if (!g) return;
        LoadedModel& gm = e.gameModel(w.model);
        if (gm.mesh && gm.mesh != e.gameCube())
        {
            g->mesh = gm.mesh; g->Scale = glm::vec3(w.scale);
            if (gm.hasMaterial) g->material = gm.material;
            std::string t = g->material.texture;
            if (t.empty() || !assetExists(t)) t = "assets/test.tga";
            g->material.texture = t; g->texId = e.gameTexture(t); g->materialSet = true;
        }
        else { g->mesh = e.gameCube(); g->Scale = glm::vec3(0.12f, 0.12f, 0.5f); }

        // Recolour the tracer pool to this weapon's bolt.
        for (int i = 0; i < kBolts; ++i)
            if (Node* b = e.gameScene().MainNode.find("bolt" + std::to_string(i)))
                b->material.color = w.bolt;
    }

public:
    void setup(Engine& e) override
    {
        Camera& cam = e.gameCamera();
        cam.pitch = 0.0f; cam.updateVectors(); // level gaze meets eye-level targets
        playerPos = glm::vec3(cam.position.x, e.gameGroundY(cam.position.x, cam.position.z), cam.position.z);
        playerVelY = 0.0f;

        GLuint tex = e.gameTexture("assets/test.tga");

        // Weapon viewmodel node; equip() below fills in the active weapon's mesh.
        // Preload both models so switching never hitches.
        for (int i = 0; i < kWeapons; ++i) e.gameModel(weapons[i].model);
        Node gun; gun.Name = "blaster";
        gun.material.color = glm::vec3(0.85f, 0.87f, 0.92f);
        gun.material.metallic = 0.6f; gun.material.roughness = 0.4f; gun.materialSet = true;
        gun.mesh = e.gameCube(); gun.texId = tex; // replaced by equip()
        e.gameScene().MainNode.Children.push_back(gun);

        // Muzzle flash: a small bright quad-cube, hidden (scale 0) until a shot.
        Node mz; mz.Name = "muzzle";
        mz.Scale = glm::vec3(0.0f);
        mz.material.color = glm::vec3(3.0f, 2.4f, 1.0f); // over-bright => glows
        mz.material.roughness = 1.0f; mz.materialSet = true;
        mz.mesh = e.gameCube(); mz.texId = tex;
        e.gameScene().MainNode.Children.push_back(mz);

        // Bolt tracer pool.
        for (int i = 0; i < kBolts; ++i)
        {
            Node b; b.Name = "bolt" + std::to_string(i);
            b.Scale = glm::vec3(0.0f);
            b.material.color = glm::vec3(0.4f, 3.0f, 1.2f); // over-bright green bolt
            b.material.roughness = 1.0f; b.materialSet = true;
            b.mesh = e.gameCube(); b.texId = tex;
            e.gameScene().MainNode.Children.push_back(b);
        }

        equip(e, weapon); // install the starting weapon's mesh + tracer colour
    }

    void onKeyPress(Engine& e, int key) override
    {
        if (key == GLFW_KEY_SPACE) jumpReq = true;
        else if (key == GLFW_KEY_1) equip(e, 0);            // blaster
        else if (key == GLFW_KEY_2) equip(e, 1);            // rifle
        else if (key == GLFW_KEY_Q) equip(e, weapon + 1);  // cycle
    }

    // Mode-side hitscan: a ray from the eye along the look direction, tested
    // against target* bounding spheres. Nearest hit within range scores.
    bool onClick(Engine& e, int button, int action) override
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return true;
        if (cooldown > 0.0f) return true;
        cooldown = weapons[weapon].fireRate;

        Camera& cam = e.gameCamera();
        glm::vec3 ro = cam.position, rd = glm::normalize(cam.front);

        // Fire feedback: kick, flash, tracer, sound.
        recoil = 1.0f; flash = 1.0f;
        Bolt& b = bolts[nextBolt];
        nextBolt = (nextBolt + 1) % kBolts;
        b.pos = muzzle(cam); b.vel = rd * 60.0f; b.life = 1.2f;
        e.gameAudio().playTone(300.0f, 0.05f);

        // Ray vs target spheres.
        std::vector<tools::DrawItem> items;
        tools::collect(e.gameScene().MainNode, glm::mat4(1.0f), items);
        const Node* hit = nullptr; float bestT = 1e9f;
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name.rfind("target", 0) != 0) continue;
            glm::vec3 bc(it.global[3]);
            float rad = 0.6f * std::max({ glm::length(glm::vec3(it.global[0])),
                                          glm::length(glm::vec3(it.global[1])),
                                          glm::length(glm::vec3(it.global[2])) });
            glm::vec3 oc = bc - ro;
            float tca = glm::dot(oc, rd);
            if (tca < 0.0f) continue;                       // behind the shooter
            float d2 = glm::dot(oc, oc) - tca * tca;
            if (d2 > rad * rad) continue;                   // ray misses the sphere
            if (tca < bestT) { bestT = tca; hit = nd; }
        }
        if (hit)
        {
            Node* n = e.gameScene().MainNode.find(hit->Name);
            bool already = n && n->material.color.g > 0.9f && n->material.color.r < 0.2f;
            if (n && !already)
            {
                n->material.color = glm::vec3(0.1f, 1.0f, 0.2f);
                score++;
                e.gameAudio().playTone(880.0f, 0.08f);
                std::cout << "Hit " << n->Name << "  score=" << score << std::endl;
            }
        }
        return true;
    }

    void update(Engine& e, float dt) override
    {
        Camera& cam = e.gameCamera();
        glm::vec3 fwd = cam.front; fwd.y = 0.0f;
        if (glm::length(fwd) > 1e-4f) fwd = glm::normalize(fwd);
        glm::vec3 rt = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 wish(0.0f);
        if (e.kW()) wish += fwd;
        if (e.kS()) wish -= fwd;
        if (e.kD()) wish += rt;
        if (e.kA()) wish -= rt;
        bool moving = glm::length(wish) > 1e-4f;
        if (moving) wish = glm::normalize(wish);
        playerPos += wish * 3.2f * dt;

        playerVelY -= 14.0f * dt;
        if (jumpReq && onGround) { playerVelY = 5.5f; onGround = false; }
        jumpReq = false;
        playerPos.y += playerVelY * dt;

        float ground = e.gameGroundY(playerPos.x, playerPos.z);
        if (playerPos.y <= ground) { playerPos.y = ground; playerVelY = 0.0f; onGround = true; }
        else onGround = false;

        collide(e.gameScene());

        // Timers.
        recoil  = std::max(0.0f, recoil  - dt / 0.12f);
        flash   = std::max(0.0f, flash   - dt / 0.05f);
        cooldown = std::max(0.0f, cooldown - dt);

        // Camera shake: a walk bob while moving on the ground, plus a vertical
        // kick from recoil. Applied to the camera position only (look stays clean,
        // so the hitscan down the crosshair is unaffected).
        float targetAmp = (moving && onGround) ? 1.0f : 0.0f;
        bobAmp += (targetAmp - bobAmp) * std::min(1.0f, 10.0f * dt);
        if (moving && onGround) bobPhase += dt * 9.0f;
        float bobV = std::sin(bobPhase * 2.0f) * 0.045f * bobAmp;
        float bobH = std::sin(bobPhase) * 0.035f * bobAmp;
        glm::vec3 camRt = cam.right();
        glm::vec3 shake = glm::vec3(0.0f, bobV, 0.0f) + camRt * bobH
                        + glm::vec3(0.0f, recoil * 0.05f, 0.0f);
        cam.position = playerPos + glm::vec3(0.0f, eyeHeight, 0.0f) + shake;

        // Pose the blaster viewmodel from the camera basis, kicked back on recoil.
        glm::vec3 cf = glm::normalize(cam.front), cr = cam.right();
        glm::vec3 cu = glm::normalize(glm::cross(cr, cf)); // true up (orthonormal)
        if (Node* g = e.gameScene().MainNode.find("blaster"))
        {
            // Local basis -> world: X=right, Y=up, Z=forward. The columns must be
            // orthonormal, else non-orthogonal columns shear the mesh and it
            // stretches as you pitch (cam.up is world up, not perpendicular to
            // front, hence the rebuilt cu). The blaster models are authored -Z
            // forward, so the barrel faces the player unless flipped 180 deg about
            // up: negate the right and forward columns (a proper rotation).
            g->useRotMatrix = true;
            g->RotMatrix = glm::mat4(glm::vec4(-cr, 0.0f), glm::vec4(cu, 0.0f),
                                     glm::vec4(-cf, 0.0f), glm::vec4(0, 0, 0, 1));
            float kick = recoil * 0.12f;
            g->Position = cam.position + cf * (kReach - kick) + cr * kRight - cu * kDown;
        }
        // Muzzle flash sits at the barrel tip, shown only during the flash window.
        if (Node* m = e.gameScene().MainNode.find("muzzle"))
        {
            m->Position = muzzle(cam);
            m->Scale = glm::vec3(flash > 0.0f ? 0.10f + 0.05f * flash : 0.0f);
        }

        // Advance and draw bolt tracers.
        for (int i = 0; i < kBolts; ++i)
        {
            Bolt& b = bolts[i];
            Node* bn = e.gameScene().MainNode.find("bolt" + std::to_string(i));
            if (b.life > 0.0f)
            {
                b.life -= dt;
                b.pos += b.vel * dt;
                if (bn)
                {
                    // Stretch the tracer along its travel direction.
                    glm::vec3 d = glm::normalize(b.vel);
                    glm::vec3 up0 = std::fabs(d.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                    glm::vec3 rr = glm::normalize(glm::cross(up0, d));
                    glm::vec3 uu = glm::cross(d, rr);
                    bn->useRotMatrix = true;
                    bn->RotMatrix = glm::mat4(glm::vec4(rr, 0.0f), glm::vec4(uu, 0.0f),
                                              glm::vec4(d, 0.0f), glm::vec4(0, 0, 0, 1));
                    bn->Position = b.pos;
                    bn->Scale = glm::vec3(0.05f, 0.05f, 0.4f);
                }
            }
            else if (bn && bn->Scale.x != 0.0f) bn->Scale = glm::vec3(0.0f);
        }
    }

    void hud(Engine& e) override
    {
        TextRenderer& t = e.gameText();
        int w = e.gameScreenW(), h = e.gameScreenH();
        t.draw("+", w * 0.5f - 5.0f, h * 0.5f + 6.0f, w, h, glm::vec3(1.0f));
        t.draw("SCORE " + std::to_string(score), w - 150.0f, 26.0f, w, h, glm::vec3(0.3f, 1.0f, 0.4f));
        t.draw(std::string("[") + weapons[weapon].name + "]", w - 150.0f, 50.0f, w, h, glm::vec3(1.0f, 0.85f, 0.3f));
        t.draw("WASD move | SPACE jump | LMB fire | 1/2 or Q weapon | TAB cursor | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }
};

} // namespace smallgine
