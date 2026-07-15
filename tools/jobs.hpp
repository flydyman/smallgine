#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <vector>
#include <algorithm>

namespace smallgine {

// Minimal thread pool with a blocking parallelFor. Used for CPU-side work
// (frustum culling, ECS updates); GL calls stay on the main thread.
// parallelFor is only called from the main thread (serially).
class JobSystem {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex m;
    std::condition_variable cv;         // wakes workers
    std::mutex idleM;
    std::condition_variable idleCv;     // signals "all tasks done" (pool-owned)
    std::atomic<int> inFlight{0};
    bool stopping = false;

    void worker()
    {
        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&] { return stopping || !tasks.empty(); });
                if (stopping && tasks.empty()) return;
                job = std::move(tasks.front());
                tasks.pop();
            }
            job();
            if (--inFlight == 0)
            {
                std::lock_guard<std::mutex> lk(idleM);
                idleCv.notify_all();
            }
        }
    }

public:
    int workerCount() const { return (int)workers.size(); }

    void start(int n = 0)
    {
        if (n <= 0) n = (int)std::thread::hardware_concurrency();
        n = std::min(std::max(n, 1), 8);
        for (int i = 0; i < n; ++i) workers.emplace_back([this] { worker(); });
    }

    // Split [0,count) into chunks and run fn(begin,end) across the pool.
    // Blocks until every chunk finishes. Completion state is pool-owned, so
    // no locals are referenced by workers after this returns.
    void parallelFor(int count, int chunk, const std::function<void(int, int)>& fn)
    {
        if (count <= 0) return;
        if (workers.empty() || count <= chunk) { fn(0, count); return; }
        int chunks = (count + chunk - 1) / chunk;
        inFlight.store(chunks);
        {
            std::lock_guard<std::mutex> lk(m);
            for (int c = 0; c < chunks; ++c)
            {
                int b = c * chunk, e = std::min(b + chunk, count);
                tasks.push([&fn, b, e] { fn(b, e); });
            }
        }
        cv.notify_all();
        std::unique_lock<std::mutex> lk(idleM);
        idleCv.wait(lk, [&] { return inFlight.load() == 0; });
    }

    void stop()
    {
        { std::lock_guard<std::mutex> lk(m); stopping = true; }
        cv.notify_all();
        for (std::thread& t : workers) if (t.joinable()) t.join();
        workers.clear();
    }
};

} // namespace smallgine
