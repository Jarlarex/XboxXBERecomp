#pragma once
#include <cstdlib>
#include <filesystem>

// diagnostics switched on by environment variables, read once
namespace diag {

// XBE_PERF set: perf log lines once a second and the read-backs named
inline bool Perf() {
    static const bool on = std::getenv("XBE_PERF") != nullptr;
    return on;
}

// XBE_TRACE_KERNEL set: kernel calls are logged with their first arguments, return value and return address, up to
// the number it holds per export (200 when it is not a number), so a stall shows the call that never returned
inline unsigned TraceKernel() {
    static const unsigned limit = [] {
        const char* v = std::getenv("XBE_TRACE_KERNEL");
        if (!v) return 0u;
        unsigned n = unsigned(std::strtoul(v, nullptr, 10));
        return n ? n : 200u;
    }();
    return limit;
}

// XBE_CAPTURE_DIR: where F3 saves a frame, empty: F3 does not capture
inline const std::filesystem::path& CaptureDir() {
    static const std::filesystem::path dir = [] {
#ifdef _WIN32
        const wchar_t* v = _wgetenv(L"XBE_CAPTURE_DIR");
#else
        const char* v = std::getenv("XBE_CAPTURE_DIR");
#endif
        return v ? std::filesystem::path(v) : std::filesystem::path();
    }();
    return dir;
}

// XBE_CAPTURE_EVERY: with XBE_CAPTURE_DIR, captures a frame every this many seconds without F3, 0 or unset: never
inline double CaptureEvery() {
    static const double seconds = [] {
        const char* v = std::getenv("XBE_CAPTURE_EVERY");
        return v ? std::strtod(v, nullptr) : 0.0;
    }();
    return seconds;
}

// XBE_CAPTURE_STEP: in a capture, the bound render target is also saved after every this many draws, to see what each
// draw added; 0 or unset: never
inline unsigned CaptureStep() {
    static const unsigned n = [] {
        const char* v = std::getenv("XBE_CAPTURE_STEP");
        return v ? unsigned(std::strtoul(v, nullptr, 10)) : 0u;
    }();
    return n;
}

}  // namespace diag
