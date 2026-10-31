#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

// ------------------------------------------------------------------ API
// Derlenen kod bu struct üzerinden oyuna erişir (scripts/exec_api.h ile birebir aynı olmalı)
struct ExecAPI {
    void (*notify)(const char*);
    void (*setSpeed)(float);
    void (*setFPS)(float);
    void (*restart)();
    bool (*getPos)(float*, float*);
    void (*setPos)(float, float);
    const char* (*levelName)();
    void (*onUpdate)(void (*)(float));
};

static const char* API_HEADER = R"(#pragma once
// Executor API - bu dosyayı elle değiştirme, mod her açılışta yeniden yazar.
#define EXEC_EXPORT extern "C" __declspec(dllexport)

struct ExecAPI {
    void (*notify)(const char*);
    void (*setSpeed)(float);
    void (*setFPS)(float);
    void (*restart)();
    bool (*getPos)(float*, float*);
    void (*setPos)(float, float);
    const char* (*levelName)();
    void (*onUpdate)(void (*)(float));
};
)";

static std::vector<void (*)(float)> g_updates;
#ifdef GEODE_IS_WINDOWS
static std::vector<HMODULE> g_libs;
#endif

static void notify(std::string const& msg, NotificationIcon icon = NotificationIcon::Info) {
    if (msg.empty()) return;
    Notification::create(msg, icon)->show();
}

static void api_notify(const char* s) { notify(s ? s : ""); }
static void api_setSpeed(float x) { CCDirector::get()->getScheduler()->setTimeScale(x); }
static void api_setFPS(float f) { if (f >= 1.f) CCDirector::get()->setAnimationInterval(1.0 / f); }
static void api_restart() { if (auto pl = PlayLayer::get()) pl->resetLevel(); }
static bool api_getPos(float* x, float* y) {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_player1) return false;
    *x = pl->m_player1->getPositionX();
    *y = pl->m_player1->getPositionY();
    return true;
}
static void api_setPos(float x, float y) {
    if (auto pl = PlayLayer::get(); pl && pl->m_player1) pl->m_player1->setPosition({x, y});
}
static const char* api_levelName() {
    static std::string name;
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_level) return "";
    name = pl->m_level->m_levelName;
    return name.c_str();
}
static void api_onUpdate(void (*fn)(float)) { if (fn) g_updates.push_back(fn); }

static ExecAPI g_api = {
    api_notify, api_setSpeed, api_setFPS, api_restart,
    api_getPos, api_setPos, api_levelName, api_onUpdate
};

// ------------------------------------------------------------ Dosya yardımcıları
static std::filesystem::path scriptsDir() {
    auto dir = Mod::get()->getSaveDir() / "scripts";
    std::error_code ec;
    std::filesystem::create_directories(dir / "build", ec);
    return dir;
}

static void writeHeader() {
    std::ofstream(scriptsDir() / "exec_api.h") << API_HEADER;
}

static void unloadAll() {
    g_updates.clear();
#ifdef GEODE_IS_WINDOWS
    for (auto h : g_libs) FreeLibrary(h);
    g_libs.clear();
#endif
}

// ------------------------------------------------------- Derle + yükle + çalıştır
static void compileAndRun(std::filesystem::path src) {
#ifndef GEODE_IS_WINDOWS
    notify("C++ executor sadece Windows'ta çalışır", NotificationIcon::Error);
    return;
#else
    static int counter = 0;
    writeHeader();
    auto dir = scriptsDir();
    auto out = dir / "build" / ("exec_" + std::to_string(GetTickCount()) + "_" + std::to_string(counter++) + ".dll");
    auto logPath = dir / "build" / "last.log";

    auto compiler = Mod::get()->getSettingValue<std::string>("compiler");
    auto flags = Mod::get()->getSettingValue<std::string>("flags");

    std::string cmd = "cmd /c \"\"" + compiler + "\" " + flags +
        " -I\"" + dir.string() + "\" \"" + src.string() + "\" -o \"" + out.string() +
        "\" > \"" + logPath.string() + "\" 2>&1\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return notify("Derleyici başlatılamadı", NotificationIcon::Error);
    }
    WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (code != 0 || !std::filesystem::exists(out)) {
        std::ifstream f(logPath);
        std::stringstream ss;
        ss << f.rdbuf();
        auto text = ss.str();
        log::error("[exec] derleme hatası:\n{}", text);
        if (text.size() > 300) text.resize(300);
        return notify("Derleme hatası (tam log: scripts/build/last.log)\n" + text, NotificationIcon::Error);
    }

    HMODULE h = LoadLibraryA(out.string().c_str());
    if (!h) return notify("DLL yüklenemedi (32/64-bit uyuşmuyor olabilir)", NotificationIcon::Error);

    auto fn = reinterpret_cast<void (*)(ExecAPI*)>(GetProcAddress(h, "run"));
    if (!fn) {
        FreeLibrary(h);
        return notify("EXEC_EXPORT void run(ExecAPI* api) bulunamadı", NotificationIcon::Error);
    }
    g_libs.push_back(h);
    fn(&g_api);
#endif
}

// Kutuya yazılan kodu şablona sarar
static void runInline(std::string const& code) {
    writeHeader();
    auto src = scriptsDir() / "build" / "inline.cpp";
    std::ofstream(src) << "#include \"exec_api.h\"\n#include <cmath>\n#include <string>\n"
                       << "EXEC_EXPORT void run(ExecAPI* api) {\n" << code << "\n}\n";
    compileAndRun(src);
}

static void runFile(std::string name) {
    if (name.empty()) name = "autorun";
    if (name.size() < 4 || name.substr(name.size() - 4) != ".cpp") name += ".cpp";
    auto p = scriptsDir() / name;
    if (!std::filesystem::exists(p)) return notify("Dosya yok: " + name, NotificationIcon::Error);
    compileAndRun(p);
}

// ------------------------------------------------------------------- Popup
class ExecutorPopup : public Popup<> {
protected:
    TextInput* m_input = nullptr;

    bool setup() override {
        this->setTitle("Executor (C++)");

        m_input = TextInput::create(280.f, "kod: api->notify(\"hi\");  /  dosya adı");
        m_input->setCommonFilter(CommonFilter::Any);
        m_input->setMaxCharCount(600);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, ccp(0, 15));

        auto run = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Run"), this, menu_selector(ExecutorPopup::onRun));
        auto file = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("File"), this, menu_selector(ExecutorPopup::onFile));
        auto clr = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Unload"), this, menu_selector(ExecutorPopup::onUnload));
        m_buttonMenu->addChildAtPosition(run, Anchor::Bottom, ccp(-90, 28));
        m_buttonMenu->addChildAtPosition(file, Anchor::Bottom, ccp(0, 28));
        m_buttonMenu->addChildAtPosition(clr, Anchor::Bottom, ccp(90, 28));
        return true;
    }

    void onRun(CCObject*) { runInline(m_input->getString()); }
    void onFile(CCObject*) { runFile(m_input->getString()); }
    void onUnload(CCObject*) { unloadAll(); notify("Yüklü kodlar kaldırıldı"); }

public:
    static ExecutorPopup* create() {
        auto ret = new ExecutorPopup();
        if (ret->initAnchored(360.f, 160.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// -------------------------------------------------------------------- Hooklar
class $modify(ExecPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;

        auto spr = ButtonSprite::create("Exec", "goldFont.fnt", "GJ_button_04.png", 0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ExecPauseLayer::onExecutor));
        btn->setID("executor-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }
    void onExecutor(CCObject*) { ExecutorPopup::create()->show(); }
};

class $modify(ExecPlayLayer, PlayLayer) {
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        for (auto fn : g_updates) fn(dt);
    }
};

$on_mod(Loaded) {
    writeHeader();
}
