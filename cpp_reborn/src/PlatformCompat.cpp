#include "PlatformCompat.h"
#include "VirtualControls.h"
#include <filesystem>
#include <iostream>
#include <vector>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <android/log.h>
#define KYS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "KYS", __VA_ARGS__)
#else
#define KYS_LOGI(...) ((void)0)
#endif

namespace fs = std::filesystem;

namespace {

bool dirLooksLikeDataRoot(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return false;
    // Prefer a root that has resource/smp or save/ranger
    if (fs::exists(root / "resource" / "smp", ec)) return true;
    if (fs::exists(root / "save" / "ranger.grp", ec)) return true;
    if (fs::exists(root / "save" / "Ranger.grp", ec)) return true;
    if (fs::is_directory(root / "resource", ec)) return true;
    return false;
}

#ifdef __ANDROID__
bool isSharedSdcardKysRoot(const std::string& root) {
    return root.find("/kys_promise") != std::string::npos &&
           root.find("/Android/data/") == std::string::npos;
}

void appendAndroidSharedKysCandidates(std::vector<fs::path>& candidates) {
    // User-editable location on shared external storage (not app-private Android/data/...).
    candidates.emplace_back("/sdcard/kys_promise");
    candidates.emplace_back("/storage/emulated/0/kys_promise");
    candidates.emplace_back("/storage/sdcard0/kys_promise");

    if (const char* ext = SDL_GetAndroidExternalStoragePath()) {
        std::string s(ext);
        const std::string marker = "/Android";
        const auto pos = s.find(marker);
        if (pos != std::string::npos) {
            candidates.emplace_back(s.substr(0, pos) + "/kys_promise");
        }
    }
}
#endif

bool EventFilter(void* /*userdata*/, SDL_Event* event) {
    if (!event) return true;
    if (!VirtualControls::handleEvent(*event)) {
        return false;
    }
    if (!PlatformCompat::transformEvent(*event)) {
        return false;  // drop consumed BACK after synthesizing cancel
    }
    return true;
}

}  // namespace

namespace PlatformCompat {

std::string discoverDataRoot() {
    std::vector<fs::path> candidates;

#ifdef __ANDROID__
    appendAndroidSharedKysCandidates(candidates);
    // App-private dirs only as last-resort fallbacks (not user-editable on newer Android).
    if (const char* internal = SDL_GetAndroidInternalStoragePath()) {
        candidates.emplace_back(internal);
        candidates.emplace_back(fs::path(internal) / "game_data");
    }
#endif

    if (char* pref = SDL_GetPrefPath("kys", "promise")) {
        candidates.emplace_back(pref);
        candidates.emplace_back(fs::path(pref) / "game_data");
        SDL_free(pref);
    }
    // SDL_GetBasePath is owned by SDL — do not free.
    if (const char* base = SDL_GetBasePath()) {
        candidates.emplace_back(base);
        candidates.emplace_back(fs::path(base) / ".." / "game_data");
    }

    // Desktop relative probe (same spirit as FileLoader)
    const char* rel[] = {
        "game_data",
        "../game_data",
        "../../game_data",
        ".",
        "..",
        "../..",
    };
    for (const char* r : rel) {
        candidates.emplace_back(r);
    }

    for (auto& c : candidates) {
        std::error_code ec;
        fs::path canon = fs::weakly_canonical(c, ec);
        if (ec) canon = c;
        if (dirLooksLikeDataRoot(canon)) {
            std::string s = canon.string();
            if (!s.empty() && s.back() != '/' && s.back() != '\\') s.push_back('/');
            std::cout << "[PlatformCompat] Data root: " << s << std::endl;
            KYS_LOGI("Data root: %s", s.c_str());
            return s;
        }
    }

    // Fallback: prefer shared SD-card kys_promise on Android
#ifdef __ANDROID__
    std::cout << "[PlatformCompat] Fallback data root: /sdcard/kys_promise/" << std::endl;
    return "/sdcard/kys_promise/";
#endif
    std::cout << "[PlatformCompat] Fallback data root: ./" << std::endl;
    return "./";
}

std::string discoverSaveDir(const std::string& dataRoot) {
    fs::path root(dataRoot);
    fs::path save = root / "save";
    std::error_code ec;
    if (!fs::exists(save, ec)) {
        fs::create_directories(save, ec);
    }
#ifdef __ANDROID__
    // Shared /sdcard/kys_promise/: read/write save next to assets (user-accessible).
    if (isSharedSdcardKysRoot(dataRoot)) {
        std::string s = save.string();
        if (!s.empty() && s.back() != '/' && s.back() != '\\') s.push_back('/');
        std::cout << "[PlatformCompat] Android save dir (shared): " << s << std::endl;
        return s;
    }
    // Legacy app-private fallback
    if (char* pref = SDL_GetPrefPath("kys", "promise")) {
        fs::path prefSave = fs::path(pref) / "save";
        fs::create_directories(prefSave, ec);
        if (fs::exists(save / "ranger.grp", ec) || fs::exists(save / "Ranger.grp", ec)) {
            SDL_free(pref);
            std::string s = save.string();
            if (!s.empty() && s.back() != '/' && s.back() != '\\') s.push_back('/');
            return s;
        }
        SDL_free(pref);
        std::string s = prefSave.string();
        if (!s.empty() && s.back() != '/' && s.back() != '\\') s.push_back('/');
        std::cout << "[PlatformCompat] Android save dir: " << s << std::endl;
        return s;
    }
#endif
    std::string s = save.string();
    if (!s.empty() && s.back() != '/' && s.back() != '\\') s.push_back('/');
    return s;
}

bool transformEvent(SDL_Event& e) {
    // Map Android BACK / ESC-like cancel into Escape key + right mouse
    // so existing UI cancel paths keep working.
    bool isBack = false;
    if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_AC_BACK) {
            isBack = true;
        }
    }
    if (!isBack) return true;

    const bool down = (e.type == SDL_EVENT_KEY_DOWN);

    SDL_Event esc{};
    esc.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    esc.key.key = SDLK_ESCAPE;
    esc.key.scancode = SDL_SCANCODE_ESCAPE;
    esc.key.down = down;
    esc.key.repeat = false;
    SDL_PushEvent(&esc);

    SDL_Event btn{};
    btn.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    btn.button.button = SDL_BUTTON_RIGHT;
    btn.button.down = down;
    btn.button.clicks = 1;
    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    btn.button.x = mx;
    btn.button.y = my;
    SDL_PushEvent(&btn);

    return false;  // drop original AC_BACK
}

void installInputCompat() {
    VirtualControls::init();
    SDL_SetEventFilter(EventFilter, nullptr);
    std::cout << "[PlatformCompat] Input compat installed (virtual pad + BACK→Esc/RMB)" << std::endl;
}

}  // namespace PlatformCompat
