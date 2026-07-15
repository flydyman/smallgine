#pragma once
#include <string>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
  #include <vector>
#else
  #include <unistd.h>
  #include <limits.h>
#endif

namespace smallgine {

// Directory containing the running executable (platform-specific).
inline std::string exeDir()
{
    std::string path;

#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0) path.assign(buf, n);
    std::string::size_type slash = path.find_last_of("/\\");
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) == 0) path = buf.data();
    std::string::size_type slash = path.find_last_of('/');
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; path = buf; }
    std::string::size_type slash = path.find_last_of('/');
#endif

    if (path.empty() || slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

// Resolve a relative asset path against the executable directory so the app
// runs from any working directory. Absolute paths pass through unchanged.
inline std::string resolvePath(const std::string& rel)
{
    if (!rel.empty() && (rel[0] == '/' || (rel.size() > 1 && rel[1] == ':'))) return rel;
    return exeDir() + "/" + rel;
}

} // namespace smallgine
