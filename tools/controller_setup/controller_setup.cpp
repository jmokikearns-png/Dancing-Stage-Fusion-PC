/* DSF-ControllerSetup -- bind keyboards, dance pads and joysticks to the two
 * PlayStation controller ports of Dancing Stage Fusion PC.
 *
 * Uses the same StepMania 2005 input stack as the game (dsfplatform: RageInput,
 * DirectInput drivers, InputMapper) and the same file, <exe dir>\config\Data\Keymaps.ini.
 * Every change is saved at once; a running game reloads the file within a second.
 *
 * Controls:
 *   click a box, or select it with the arrow keys and press Enter, then press the key
 *   or pad button to assign to it;  Esc cancels.
 *   Delete / Backspace / right-click clears the selected box.
 *   Rows light up while their button is held: use this to test a pad.
 */
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "dsfplatform.h"

namespace {

const int kSlots = 3;               /* StepMania shows 3 of its 5 slots per controller */
const int kRows = dsf::NUM_PS1_BUTTONS;
const char *kTitle = "Dancing Stage Fusion PC - Controller Setup";

const char *kRowLabel[kRows] = {
    "Up", "Down", "Left", "Right",
    "Cross  (X)", "Circle  (O)", "Square", "Triangle",
    "L1", "L2", "R1", "R2",
    "Start", "Select",
};

/* Layout (client pixels). */
const int kMargin = 16, kLabelW = 120, kCellW = 112, kCellH = 26, kPadGap = 18;
const int kHeaderY = 52, kGridY = 100;

HWND g_wnd;
int g_sel_pad = 0, g_sel_row = 0, g_sel_slot = 0;
bool g_ready = false;      /* StepMania input is up; before that, nothing may touch the mapping */
bool g_capturing = false;
DWORD g_ignore_keys_until = 0; /* the bound key's own WM_KEYDOWN must not navigate */
std::string g_status = "Click a box, then press the key or pad button for it.";
std::vector<std::string> g_devices;
std::string g_keymaps_path;
bool g_pressed[dsf::NUM_PADS][kRows];
HFONT g_font, g_font_bold, g_font_small;

RECT cell_rect(int pad, int row, int slot)
{
    const int x = kMargin + kLabelW + pad * (kSlots * kCellW + kPadGap) + slot * kCellW;
    const int y = kGridY + row * kCellH;
    RECT r = {x, y, x + kCellW - 4, y + kCellH - 4};
    return r;
}

RECT reset_button_rect()
{
    const int y = kGridY + kRows * kCellH + 12;
    RECT r = {kMargin, y, kMargin + 190, y + 30};
    return r;
}

RECT close_button_rect()
{
    const int y = kGridY + kRows * kCellH + 12;
    RECT r = {kMargin + 200, y, kMargin + 300, y + 30};
    return r;
}

std::string pretty(const std::string &di)
{
    if (di.empty())
        return "-";
    if (di.compare(0, 4, "Key_") == 0) {
        std::string k = di.substr(4);
        if (!k.empty())
            k[0] = char(toupper((unsigned char) k[0]));
        return "Key " + k;
    }
    if (di.compare(0, 3, "Joy") == 0) {
        const size_t us = di.find('_');
        if (us != std::string::npos) {
            std::string rest = di.substr(us + 1);
            if (!rest.empty() && rest[0] == 'B' && rest.size() > 1 && isdigit((unsigned char) rest[1]))
                rest = "Button " + rest.substr(1);
            return "Pad " + di.substr(3, us - 3) + " " + rest;
        }
    }
    return di;
}

void flush_input()
{
    dsf::Update(0.0f);
    std::vector<dsf::PadEvent> pe;
    std::vector<dsf::RawEvent> re;
    dsf::GetEvents(pe, re);
}

void save()
{
    dsf::SaveMapping();
}

void start_capture()
{
    flush_input(); /* drop the Enter / click that started this */
    g_capturing = true;
    char buf[160];
    std::snprintf(buf, sizeof buf, "Player %d %s, box %d: press a key or pad button (Esc cancels)",
                  g_sel_pad + 1, kRowLabel[g_sel_row], g_sel_slot + 1);
    g_status = buf;
    InvalidateRect(g_wnd, nullptr, FALSE);
}

void clear_selected()
{
    dsf::ClearMapping(g_sel_pad, dsf::Ps1Button(g_sel_row), g_sel_slot);
    save();
    g_status = "Cleared.";
    InvalidateRect(g_wnd, nullptr, FALSE);
}

void assign(const std::string &device_input)
{
    g_capturing = false;
    g_ignore_keys_until = GetTickCount() + 300;
    if (device_input == "Key_escape") {
        g_status = "Cancelled.";
    } else if (dsf::SetMapping(g_sel_pad, dsf::Ps1Button(g_sel_row), g_sel_slot, device_input)) {
        save();
        g_status = "Player " + std::to_string(g_sel_pad + 1) + " " + kRowLabel[g_sel_row] + " = " +
                   pretty(device_input) + "  (saved)";
    } else {
        g_status = "Could not assign " + device_input + ".";
    }
    InvalidateRect(g_wnd, nullptr, FALSE);
}

void tick(float dt)
{
    dsf::Update(dt);
    std::vector<dsf::PadEvent> pe;
    std::vector<dsf::RawEvent> re;
    dsf::GetEvents(pe, re);
    if (g_capturing) {
        for (const dsf::RawEvent &e : re) {
            if (e.bPressed) {
                assign(e.sDeviceInput);
                flush_input();
                break;
            }
        }
    }
    bool changed = false;
    for (int p = 0; p < dsf::NUM_PADS; ++p)
        for (int r = 0; r < kRows; ++r) {
            const bool down = dsf::IsPressed(p, dsf::Ps1Button(r));
            if (down != g_pressed[p][r]) {
                g_pressed[p][r] = down;
                changed = true;
            }
        }
    if (changed)
        InvalidateRect(g_wnd, nullptr, FALSE);
}

void fill(HDC dc, const RECT &r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void text(HDC dc, int x, int y, const std::string &s, COLORREF c, HFONT f)
{
    SelectObject(dc, f);
    SetTextColor(dc, c);
    TextOutA(dc, x, y, s.c_str(), int(s.size()));
}

void paint(HDC out, const RECT &client)
{
    HDC dc = CreateCompatibleDC(out);
    HBITMAP bmp = CreateCompatibleBitmap(out, client.right, client.bottom);
    HGDIOBJ old_bmp = SelectObject(dc, bmp);
    SetBkMode(dc, TRANSPARENT);

    const COLORREF bg = RGB(18, 22, 30), panel = RGB(34, 40, 54), panel_hi = RGB(52, 60, 80),
                   lit = RGB(40, 110, 60), sel = RGB(70, 120, 210), cap = RGB(200, 160, 30),
                   fg = RGB(230, 232, 238), dim = RGB(150, 156, 170);
    fill(dc, client, bg);

    text(dc, kMargin, 12, kTitle, fg, g_font_bold);
    for (int p = 0; p < dsf::NUM_PADS; ++p) {
        const RECT c0 = cell_rect(p, 0, 0);
        text(dc, c0.left, kHeaderY, "Player " + std::to_string(p + 1) + "  (controller port " +
                                        std::to_string(p + 1) + ")", fg, g_font_bold);
        for (int s = 0; s < kSlots; ++s) {
            const RECT c = cell_rect(p, 0, s);
            text(dc, c.left + 4, kHeaderY + 24, s == 2 ? "Box 3 (default)" : "Box " + std::to_string(s + 1),
                 dim, g_font_small);
        }
    }

    for (int r = 0; r < kRows; ++r) {
        const RECT c0 = cell_rect(0, r, 0);
        text(dc, kMargin, c0.top + 3, kRowLabel[r], fg, g_font);
        for (int p = 0; p < dsf::NUM_PADS; ++p)
            for (int s = 0; s < kSlots; ++s) {
                const RECT c = cell_rect(p, r, s);
                const bool selected = p == g_sel_pad && r == g_sel_row && s == g_sel_slot;
                COLORREF col = g_pressed[p][r] ? lit : panel;
                if (selected)
                    col = g_capturing ? cap : sel;
                fill(dc, c, col);
                std::string label = selected && g_capturing ? "press..." :
                    pretty(dsf::GetMapping(p, dsf::Ps1Button(r), s));
                text(dc, c.left + 6, c.top + 3, label, selected || g_pressed[p][r] ? fg : (label == "-" ? dim : fg),
                     g_font);
            }
    }

    const RECT rb = reset_button_rect(), cb = close_button_rect();
    fill(dc, rb, panel_hi);
    fill(dc, cb, panel_hi);
    text(dc, rb.left + 10, rb.top + 6, "Reset all to defaults", fg, g_font);
    text(dc, cb.left + 28, cb.top + 6, "Close", fg, g_font);

    int y = rb.bottom + 14;
    text(dc, kMargin, y, g_status, g_capturing ? cap : fg, g_font);
    y += 26;
    text(dc, kMargin, y, "Click a box (or arrows + Enter), then press the key / pad button.  Delete or right-click clears.  "
                         "Rows light up while held.", dim, g_font_small);
    y += 18;
    text(dc, kMargin, y, "Changes are saved at once and a running game picks them up within a second.", dim, g_font_small);
    y += 24;
    text(dc, kMargin, y, "Devices found:", fg, g_font_small);
    for (const std::string &d : g_devices) {
        y += 17;
        text(dc, kMargin + 12, y, d, dim, g_font_small);
    }
    y += 22;
    text(dc, kMargin, y, "Mapping file: " + g_keymaps_path, dim, g_font_small);

    BitBlt(out, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

bool hit(const RECT &r, int x, int y)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

bool cell_at(int x, int y, int *pad, int *row, int *slot)
{
    for (int p = 0; p < dsf::NUM_PADS; ++p)
        for (int r = 0; r < kRows; ++r)
            for (int s = 0; s < kSlots; ++s)
                if (hit(cell_rect(p, r, s), x, y)) {
                    *pad = p;
                    *row = r;
                    *slot = s;
                    return true;
                }
    return false;
}

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT client;
        GetClientRect(h, &client);
        /* ShowWindow/UpdateWindow paint synchronously on Windows, before
         * dsf::Init() has created the input mapper. */
        if (g_ready)
            paint(dc, client);
        else
            FillRect(dc, &client, (HBRUSH) GetStockObject(BLACK_BRUSH));
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        if (!g_ready)
            return 0;
        const int x = short(LOWORD(lp)), y = short(HIWORD(lp));
        int p, r, s;
        if (cell_at(x, y, &p, &r, &s)) {
            g_sel_pad = p;
            g_sel_row = r;
            g_sel_slot = s;
            if (msg == WM_RBUTTONDOWN) {
                g_capturing = false;
                clear_selected();
            } else {
                start_capture();
            }
        } else if (msg == WM_LBUTTONDOWN && hit(reset_button_rect(), x, y)) {
            g_capturing = false;
            dsf::ResetMappingToDefaults();
            save();
            g_status = "All bindings reset to the defaults.";
            InvalidateRect(h, nullptr, FALSE);
        } else if (msg == WM_LBUTTONDOWN && hit(close_button_rect(), x, y)) {
            DestroyWindow(h);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (!g_ready || g_capturing || int(GetTickCount() - g_ignore_keys_until) < 0)
            return 0; /* the capture takes the next DirectInput press instead */
        switch (wp) {
        case VK_UP: g_sel_row = (g_sel_row + kRows - 1) % kRows; break;
        case VK_DOWN: g_sel_row = (g_sel_row + 1) % kRows; break;
        case VK_LEFT:
            if (g_sel_slot > 0) --g_sel_slot;
            else if (g_sel_pad > 0) { --g_sel_pad; g_sel_slot = kSlots - 1; }
            break;
        case VK_RIGHT:
            if (g_sel_slot < kSlots - 1) ++g_sel_slot;
            else if (g_sel_pad < dsf::NUM_PADS - 1) { ++g_sel_pad; g_sel_slot = 0; }
            break;
        case VK_RETURN: start_capture(); return 0;
        case VK_DELETE: case VK_BACK: clear_selected(); return 0;
        case VK_ESCAPE: DestroyWindow(h); return 0;
        default: return 0;
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

std::string exe_dir()
{
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    const size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
}

} // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = "DSFControllerSetup";
    RegisterClassA(&wc);

    const int width = 2 * kMargin + kLabelW + 2 * kSlots * kCellW + kPadGap + 8;
    const int height = kGridY + kRows * kCellH + 250;
    RECT wr = {0, 0, width, height};
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_wnd = CreateWindowA(wc.lpszClassName, kTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
                          nullptr, nullptr, inst, nullptr);
    g_font = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font_bold = CreateFontA(-17, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font_small = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);
    SetForegroundWindow(g_wnd);

    const std::string config_dir = exe_dir() + "\\config";
    CreateDirectoryA(config_dir.c_str(), nullptr);
    dsf::SetHostWindow(g_wnd);
    dsf::Config cfg;
    cfg.sConfigDir = config_dir;
    cfg.bOpenWindow = false;
    cfg.bThreadedInput = true;
    cfg.bLogToStdout = false;
    char arg0[] = "DSF-ControllerSetup";
    char *argv[] = {arg0, nullptr};
    std::string error;
    if (!dsf::Init(1, argv, cfg, error)) {
        MessageBoxA(g_wnd, ("Input could not be started:\n" + error).c_str(), kTitle, MB_ICONERROR);
        return 1;
    }
    dsf::SaveMapping();
    g_keymaps_path = dsf::KeymapsPath();
    dsf::GetDevices(g_devices);
    g_ready = true;
    InvalidateRect(g_wnd, nullptr, FALSE);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    for (;;) {
        MSG m;
        while (PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) {
                dsf::Shutdown(); /* saves the mapping, as StepMania does */
                return 0;
            }
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
        QueryPerformanceCounter(&now);
        const float dt = float(now.QuadPart - last.QuadPart) / float(freq.QuadPart);
        last = now;
        tick(dt);
        Sleep(5);
    }
}
