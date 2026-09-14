#pragma once
#include <filesystem>
#include <fstream>
#include <string_view>
#include <mutex>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mkw::platform {
// The caller serializes read/modify/write. A failed write leaves the old file.
inline bool AtomicWriteText(const std::filesystem::path& path, std::string_view text) {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    auto temporary = path;
    temporary += ".pending";
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.flush();
        if (!output) { output.close(); std::filesystem::remove(temporary, ec); return false; }
        output.close();
        if (!output) { std::filesystem::remove(temporary, ec); return false; }
    }
#ifdef _WIN32
    const bool ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, ec);
    const bool ok = !ec;
#endif
    if (!ok) std::filesystem::remove(temporary, ec);
    return ok;
}
}
