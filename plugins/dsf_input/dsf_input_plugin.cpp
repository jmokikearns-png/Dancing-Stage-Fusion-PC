/* dsf.input.stepmania -- PS1 controller ports driven by StepMania 2005's input stack.
 *
 * The game keeps reading its pads exactly as on a PlayStation (SIO digital pad
 * protocol). What changes is where the button bits come from: instead of the
 * runtime's own keyboard/gamepad mapping, every frame takes the pad word that
 * StepMania's RageInput + InputMapper (dsfplatform, DirectInput on Windows)
 * resolve from the player's keyboard, dance pad or joystick, using the bindings
 * in <exe>/config/Data/Keymaps.ini.
 *
 * Mechanics:
 *  - Registered as an activation plugin; the dsf.input.stepmania mod package
 *    (enabled by default) selects it. On activation it installs a pad transform
 *    (mod_plugins.h, stage 2b) for players 1 and 2, which replaces the resolved
 *    pad with the StepMania pad word and presents a digital pad.
 *  - dsfplatform starts lazily on the first transform call that finds the game
 *    window: DirectInput needs that top-level window, and only accepts keyboard
 *    input while it is in the foreground (StepMania behaviour).
 *  - Keymaps.ini is reloaded when its timestamp changes, so the controller setup
 *    tool can be used while the game runs.
 *  - If dsfplatform cannot start, the transform declines and the runtime's own
 *    stock pad is delivered instead, so the game stays playable.
 *  - F1 (seen by StepMania's keyboard driver, so only while the game window is in
 *    front) opens DSF-ControllerSetup.exe from the game folder.
 */
#include "mod_plugins.h"
#include "dsfplatform.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

enum class State { NotStarted, Ready, Failed };

State g_state = State::NotStarted;
LARGE_INTEGER g_freq, g_last;
std::string g_keymaps_path;
FILETIME g_keymaps_mtime = {};
DWORD g_last_reload_check_ms = 0;
FILE *g_log = nullptr;

std::string exe_dir()
{
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    const size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
}

void log_line(const char *fmt, ...)
{
    if (!g_log)
        return;
    va_list va;
    va_start(va, fmt);
    std::vfprintf(g_log, fmt, va);
    va_end(va);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

struct FindWindowCtx {
    DWORD pid;
    HWND sdl;
    HWND any;
};

BOOL CALLBACK find_window_cb(HWND h, LPARAM lp)
{
    FindWindowCtx *c = reinterpret_cast<FindWindowCtx *>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER) != nullptr)
        return TRUE;
    char cls[64] = "";
    GetClassNameA(h, cls, sizeof cls);
    if (std::strcmp(cls, "SDL_app") == 0) {
        c->sdl = h;
        return FALSE;
    }
    if (!c->any)
        c->any = h;
    return TRUE;
}

HWND find_game_window()
{
    FindWindowCtx c = {GetCurrentProcessId(), nullptr, nullptr};
    EnumWindows(find_window_cb, reinterpret_cast<LPARAM>(&c));
    return c.sdl ? c.sdl : c.any;
}

bool keymaps_mtime(FILETIME *out)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(g_keymaps_path.c_str(), GetFileExInfoStandard, &d))
        return false;
    *out = d.ftLastWriteTime;
    return true;
}

void try_start()
{
    const HWND window = find_game_window();
    if (!window)
        return; /* no window yet: the stock pad is delivered meanwhile */

    const std::string config_dir = exe_dir() + "\\config";
    CreateDirectoryA(config_dir.c_str(), nullptr);
    g_log = std::fopen((config_dir + "\\dsf_input.log").c_str(), "w");

    dsf::SetHostWindow(window);
    dsf::Config cfg;
    cfg.sConfigDir = config_dir;
    cfg.bOpenWindow = false;
    cfg.bThreadedInput = true;
    cfg.bLogToStdout = false;
    char arg0[] = "DancingStageFusionPC";
    char *argv[] = {arg0, nullptr};
    std::string error;
    if (!dsf::Init(1, argv, cfg, error)) {
        log_line("StepMania input failed to start: %s (using the runtime's own controls)", error.c_str());
        g_state = State::Failed;
        return;
    }

    /* Write the mapping out once so the file (with defaults and any auto-mapped
     * pads) exists for the setup tool, then remember its timestamp. */
    dsf::SaveMapping();
    g_keymaps_path = dsf::KeymapsPath();
    keymaps_mtime(&g_keymaps_mtime);

    std::vector<std::string> devices;
    dsf::GetDevices(devices);
    log_line("StepMania input started; mapping file %s", g_keymaps_path.c_str());
    for (const std::string &d : devices)
        log_line("  device %s", d.c_str());

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_last);
    g_state = State::Ready;
}

void open_controller_setup()
{
    const std::string tool = exe_dir() + "\\DSF-ControllerSetup.exe";
    const HINSTANCE r = ShellExecuteA(nullptr, "open", tool.c_str(), nullptr, exe_dir().c_str(), SW_SHOWNORMAL);
    log_line("F1: open %s -> %s", tool.c_str(), INT_PTR(r) > 32 ? "ok" : "failed");
}

void poll_input()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = float(now.QuadPart - g_last.QuadPart) / float(g_freq.QuadPart);
    g_last = now;
    if (dt < 0.0f || dt > 1.0f)
        dt = 0.0f;
    dsf::Update(dt);

    std::vector<dsf::PadEvent> pad_events;
    std::vector<dsf::RawEvent> raw_events;
    dsf::GetEvents(pad_events, raw_events); /* drained every frame; state is read below */
    for (const dsf::RawEvent &e : raw_events) {
        if (e.bPressed && e.sDeviceInput == "Key_F1")
            open_controller_setup();
    }

    const DWORD ms = GetTickCount();
    if (ms - g_last_reload_check_ms >= 1000) {
        g_last_reload_check_ms = ms;
        FILETIME t;
        if (keymaps_mtime(&t) && CompareFileTime(&t, &g_keymaps_mtime) != 0) {
            g_keymaps_mtime = t;
            dsf::LoadMapping();
            log_line("Keymaps.ini changed: mapping reloaded");
        }
    }
}

int pad_transform(const PSXModPadFrame *frame, PSXModPadOutput *out)
{
    if (g_state == State::NotStarted)
        try_start();
    if (g_state != State::Ready || frame->player > 1)
        return 0;
    if (frame->player == 0)
        poll_input();
    out->buttons = dsf::GetPadWord(int(frame->player));
    out->type = PSX_MOD_PAD_DIGITAL;
    out->lx = out->ly = out->rx = out->ry = 0x80;
    return 1;
}

const PSXModPadTransform g_transform = {
    sizeof(PSXModPadTransform),
    PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DIGITAL),
    PSX_MOD_PAD_DIGITAL,
    pad_transform,
};

void activate()
{
    psx_mod_set_pad_transform(0, &g_transform);
    psx_mod_set_pad_transform(1, &g_transform);
}

} // namespace

PSX_MOD_CONSTRUCTOR(dsf_register_input_plugin)
{
    (void)psx_mod_register_activation_plugin("dsf.input.stepmania", activate);
}
