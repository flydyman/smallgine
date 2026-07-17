#pragma once

namespace smallgine {

class Engine;
class DebugLines;

// A pluggable game mode. The engine owns rendering, input plumbing, camera, and
// its built-in sandbox; a mode supplies demo-specific objects and gameplay via
// these hooks. Demos live entirely in their own folder and implement this.
class IGameMode
{
public:
    virtual ~IGameMode() = default;

    // Spawn objects / configure the camera once, after the scene is loaded.
    virtual void setup(Engine&) {}
    // Per-frame gameplay + camera control (replaces the default free-fly camera).
    virtual void update(Engine&, float /*dt*/) {}
    // A key was pressed (discrete actions; held movement is read via Engine getters).
    virtual void onKeyPress(Engine&, int /*key*/) {}
    // A mouse button event; return true if handled (suppresses the default pick).
    virtual bool onClick(Engine&, int /*button*/, int /*action*/) { return false; }
    // Draw the mode's HUD (called in the 2D overlay pass).
    virtual void hud(Engine&) {}
    // Release any GPU resources the mode created (called before context teardown).
    virtual void teardown(Engine&) {}

    // Render customization.
    virtual bool drawSkybox() const { return true; }  // false => keep the cleared background
    virtual bool clearBlack() const { return false; } // true => black background (space)
    virtual float cinematic() const { return 1.0f; }  // 0 => no motion blur / DoF smear

    // Collider debug view (F3). Return true to take over the drawing from the
    // engine's generic per-node AABBs (e.g. to show an oriented collider that
    // rotates with the object), then draw into `dl` in debugColliders().
    virtual bool drawsColliders() const { return false; }
    virtual void debugColliders(Engine&, DebugLines& /*dl*/) {}
};

} // namespace smallgine
