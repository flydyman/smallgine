#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <utility>

namespace smallgine {

// Per-frame CPU section timer. mark(label) records the time spent since the
// previous mark under `label`; endFrame() records the whole-frame total.
// No-op unless enabled, so it costs nothing when the overlay is off.
class Profiler {
private:
    using Clock = std::chrono::steady_clock;
    bool enabled = false;
    Clock::time_point frameStart, lastMark;
    std::vector<std::pair<std::string, double>> sections;
    std::vector<std::pair<std::string, double>> shown; // last completed frame
    double frameTotal = 0.0, gpuMs = 0.0;

    static double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

public:
    void toggle() { enabled = !enabled; }
    bool on() const { return enabled; }

    void beginFrame()
    {
        if (!enabled) return;
        sections.clear();
        frameStart = lastMark = Clock::now();
    }
    void mark(const std::string& label)
    {
        if (!enabled) return;
        auto now = Clock::now();
        sections.push_back({ label, ms(now - lastMark) });
        lastMark = now;
    }
    void endFrame()
    {
        if (!enabled) return;
        frameTotal = ms(Clock::now() - frameStart);
        shown = sections;
    }
    void setGpuMs(double g) { gpuMs = g; }

    const std::vector<std::pair<std::string, double>>& report() const { return shown; }
    double totalMs() const { return frameTotal; }
    double gpu() const { return gpuMs; }
};

} // namespace smallgine
