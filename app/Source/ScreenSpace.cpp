#include "ScreenSpace.h"
#include <cstdio>
// PORTED from MeterBar v0.18.1 — READ THIS BEFORE TOUCHING THE GUARD
// (the MeterBar v0.14 lesson, inherited on purpose): this translation
// unit deliberately includes NO JUCE header — it is driven entirely
// through void* window handles the caller passes in. JUCE_WINDOWS
// exists only inside JUCE's own headers, so a `#if JUCE_WINDOWS` guard
// here would read 0 in EVERY build and the stub branch below would ship
// on Windows — that exact bug kept WASAPI loopback out of every MeterBar
// binary from v0.10 to v0.13. The guard therefore tests the compiler's
// own predefined target macro: MSVC and MinGW both predefine _WIN32 for
// Windows targets, no header needed. The build's verification greps THIS
// file's preprocessed Windows-target output for the "appbar active"
// status string, which exists only in the real branch — proof the
// branch is compiled in, not just present in source.
#if defined(_WIN32)
 #define SESSIONBAR_SCREENSPACE_WINDOWS 1
#else
 #define SESSIONBAR_SCREENSPACE_WINDOWS 0
#endif

#if SESSIONBAR_SCREENSPACE_WINDOWS
#ifndef NOMINMAX
 #define NOMINMAX // keep windows.h's min/max macros out of the std:: calls below
#endif
 #include <windows.h>
 #include <shellapi.h>  // SHAppBarMessage / APPBARDATA
 #include <commctrl.h>  // SetWindowSubclass (AppBar callback messages)
 #include <dwmapi.h>    // DWMWA_CLOAKED (arrange sweep skips cloaked windows)

// ---- AppBar runtime state ----------------------------------------------
// Single-instance app with exactly one dockable window, so the one live
// registration is kept at file scope (MeterBar's design): the subclass
// callback below is a plain function pointer and needs somewhere to
// stand. The ScreenSpace methods are the only writers.
namespace {
struct AppBarState {
    HWND hwnd = nullptr;
    UINT callbackMsg = WM_APP + 0x5B; // private app message for ABN_* notices
    UINT edge = ABE_TOP;
    bool registered = false;
    RECT requested{}; // the rect the CALLER last asked for — idempotence is judged on THIS
    RECT applied{};   // the rect Windows last accepted via ABM_SETPOS (may be trimmed)
};
AppBarState g_appBar;

// Ask for the position and set it ONLY if the system-granted rect
// differs from what is already applied. (MeterBar v0.18's handler
// called ABM_SETPOS unconditionally — and an ABM_SETPOS generates fresh
// ABN_POSCHANGED notifications, so the handler could re-trigger itself
// and keep the work area recalculating in a loop. With the equality
// gate, a notification that changes nothing costs one ABM_QUERYPOS and
// zero window movement.)
void setAppBarPosIfChanged() {
    APPBARDATA abd{}; abd.cbSize = sizeof abd; abd.hWnd = g_appBar.hwnd;
    abd.uEdge = g_appBar.edge; abd.rc = g_appBar.requested;
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    // Our span along the edge and the anchored edge coordinate are the
    // ask; the system may trim the strip's thickness only.
    abd.rc.left = g_appBar.requested.left; abd.rc.right = g_appBar.requested.right;
    if (g_appBar.edge == ABE_TOP) abd.rc.top = g_appBar.requested.top;
    else abd.rc.bottom = g_appBar.requested.bottom;
    if (abd.rc.left == g_appBar.applied.left && abd.rc.top == g_appBar.applied.top &&
        abd.rc.right == g_appBar.applied.right && abd.rc.bottom == g_appBar.applied.bottom)
        return; // already exactly this bar — touching it again only broadcasts noise
    SHAppBarMessage(ABM_SETPOS, &abd);
    g_appBar.applied = abd.rc;
}

// ABN_POSCHANGED: another bar (or the taskbar) changed the available
// space — the documented response is to re-ask; we re-SET only when
// the answer actually moved us (see setAppBarPosIfChanged above).
LRESULT CALLBACK appBarSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR subclassId, DWORD_PTR refData) {
    (void)subclassId; (void)refData;
    if (msg == g_appBar.callbackMsg && wp == ABN_POSCHANGED && g_appBar.registered)
        setAppBarPosIfChanged();
    return DefSubclassProc(hwnd, msg, wp, lp);
}
} // namespace

struct ScreenSpace::Impl {
    std::string status = "not registered";
    std::vector<SavedWindow> saved;
};

bool ScreenSpace::reserve(void* hwndV, int edge, int physX, int physY, int physW, int physH) {
    HWND hwnd = static_cast<HWND>(hwndV);
    if (!hwnd || physW <= 0 || physH <= 0) return g_appBar.registered;
    const UINT uEdge = edge == 1 ? ABE_BOTTOM : ABE_TOP;
    const RECT want{ physX, physY, physX + physW, physY + physH };
    // THE MeterBar v0.18 FLICKER, fixed in v0.18.1 and inherited here:
    // idempotence is judged on the REQUESTED rect. v0.18 compared
    // `want` against the rect Windows GRANTED (post-QUERYPOS, which the
    // system may trim) — the two could never compare equal, so every
    // sync tore the AppBar down and rebuilt it at the UI timer's rate.
    if (g_appBar.registered && g_appBar.hwnd == hwnd && g_appBar.edge == uEdge &&
        want.left == g_appBar.requested.left && want.top == g_appBar.requested.top &&
        want.right == g_appBar.requested.right && want.bottom == g_appBar.requested.bottom)
        return true; // already exactly this bar — nothing to do
    if (g_appBar.registered) release(); // moved / resized / other edge or monitor: clean re-register
    APPBARDATA abd{}; abd.cbSize = sizeof abd; abd.hWnd = hwnd;
    abd.uCallbackMessage = g_appBar.callbackMsg;
    if (!SHAppBarMessage(ABM_NEW, &abd)) {
        impl_->status = "appbar registration refused by Windows";
        return false;
    }
    g_appBar.hwnd = hwnd;
    g_appBar.edge = uEdge;
    g_appBar.requested = want;
    g_appBar.applied = RECT{}; // nothing applied yet — setAppBarPosIfChanged always places the first position
    SetWindowSubclass(hwnd, appBarSubclassProc, 1, 0);
    g_appBar.registered = true; // live BEFORE the first SETPOS, so a POSCHANGED landing mid-call sees a consistent bar
    setAppBarPosIfChanged();
    char buf[96];
    std::snprintf(buf, sizeof buf, "appbar active - %s strip %ld px reserved",
                  uEdge == ABE_TOP ? "top" : "bottom",
                  (long)(g_appBar.applied.bottom - g_appBar.applied.top));
    impl_->status = buf;
    return true;
}

void ScreenSpace::release() {
    if (!g_appBar.registered || !g_appBar.hwnd) { g_appBar.registered = false; return; }
    APPBARDATA abd{}; abd.cbSize = sizeof abd; abd.hWnd = g_appBar.hwnd;
    SHAppBarMessage(ABM_REMOVE, &abd);
    RemoveWindowSubclass(g_appBar.hwnd, appBarSubclassProc, 1);
    g_appBar.hwnd = nullptr;
    g_appBar.registered = false;
    g_appBar.requested = RECT{};
    g_appBar.applied = RECT{};
    impl_->status = "not registered";
}

bool ScreenSpace::isReserved() const { return g_appBar.registered; }

// ---- Arrange sweep ------------------------------------------------------
namespace {
struct ArrangeMove { HWND hwnd; sessionbar::IntRect from, to; };
struct ArrangeCtx {
    HMONITOR mon; HWND exclude; int strip; int edge;
    sessionbar::IntRect monRect;
    std::vector<ArrangeMove> moves;
};
BOOL CALLBACK arrangeEnumProc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<ArrangeCtx*>(lp);
    if (hwnd == ctx->exclude || hwnd == GetShellWindow()) return TRUE;
    // Minimised windows have no meaningful on-screen rect; MAXIMISED
    // windows are deliberately left alone — the AppBar reservation owns
    // them (Windows sizes them against the shrunken work area).
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || IsZoomed(hwnd)) return TRUE;
    // Never fight the user: a window in an interactive move/size loop
    // (being dragged or edge-resized at this very moment) is skipped
    // outright (MeterBar v0.18.1 guard).
    {
        GUITHREADINFO gti{}; gti.cbSize = sizeof gti;
        const DWORD tid = GetWindowThreadProcessId(hwnd, nullptr);
        if (tid && GetGUIThreadInfo(tid, &gti) && (gti.flags & GUI_INMOVESIZE) && gti.hwndActive == hwnd)
            return TRUE;
    }
    if (MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) != ctx->mon) return TRUE;
    DWORD cloaked = 0; // other virtual desktops / suspended UWP ghosts
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked)
        return TRUE;
    if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    RECT r{};
    if (!GetWindowRect(hwnd, &r)) return TRUE;
    const sessionbar::IntRect wr{ r.left, r.top, r.right - r.left, r.bottom - r.top };
    if (wr.w <= 0 || wr.h <= 0) return TRUE;
    // Fit FIRST, move later: mutating windows mid-enumeration can disturb
    // the enumeration itself, so this pass only collects the plan.
    const sessionbar::IntRect fit = ctx->edge == 1
        ? sessionbar::arrangeFitAbove(wr, ctx->monRect, ctx->strip)
        : sessionbar::arrangeFit(wr, ctx->monRect, ctx->strip);
    if (fit != wr) ctx->moves.push_back({ hwnd, wr, fit });
    return TRUE;
}
} // namespace

int ScreenSpace::arrange(void* hwndV, int stripPhysPx, int edge) {
    HWND hwnd = static_cast<HWND>(hwndV);
    if (!hwnd || stripPhysPx <= 0) {
        impl_->status = "arrange: no strip to clear (dock the bar at a monitor edge first)";
        return 0;
    }
    const HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{}; mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(mon, &mi)) { impl_->status = "arrange: monitor lookup failed"; return 0; }
    ArrangeCtx ctx{ mon, hwnd, stripPhysPx, edge,
                    { mi.rcMonitor.left, mi.rcMonitor.top,
                      mi.rcMonitor.right - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top }, {} };
    EnumWindows(arrangeEnumProc, reinterpret_cast<LPARAM>(&ctx));
    int moved = 0;
    for (auto& mv : ctx.moves) {
        // Snapshots are WRITE-ONCE (MeterBar v0.18.1): a window whose
        // original rect is already recorded keeps that original — a
        // second sweep must never "remember" a post-first-sweep
        // position as home. Only restore() clears the record.
        bool known = false;
        for (auto& s : impl_->saved) if (s.hwnd == mv.hwnd) { known = true; break; }
        if (!known) impl_->saved.push_back({ mv.hwnd, mv.from });
        if (SetWindowPos(mv.hwnd, nullptr, mv.to.x, mv.to.y, mv.to.w, mv.to.h,
                         SWP_NOZORDER | SWP_NOACTIVATE))
            ++moved;
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "arranged %d window(s) clear of the bar", moved);
    impl_->status = buf;
    return moved;
}

int ScreenSpace::restore() {
    int n = 0;
    for (auto it = impl_->saved.rbegin(); it != impl_->saved.rend(); ++it) {
        HWND hwnd = static_cast<HWND>(it->hwnd);
        if (IsWindow(hwnd) && SetWindowPos(hwnd, nullptr, it->rect.x, it->rect.y,
                                            it->rect.w, it->rect.h,
                                            SWP_NOZORDER | SWP_NOACTIVATE))
            ++n;
    }
    impl_->saved.clear();
    char buf[96];
    std::snprintf(buf, sizeof buf, "restored %d window(s)", n);
    impl_->status = buf;
    return n;
}

std::string ScreenSpace::statusInfo() const { return impl_->status; }

#else // ---- non-Windows stub: same idiom as MeterBar's ScreenSpace ------
struct ScreenSpace::Impl {
    std::string status = "unavailable (non-Windows build)";
    std::vector<SavedWindow> saved;
};
bool ScreenSpace::reserve(void*, int, int, int, int, int) { return false; }
void ScreenSpace::release() {}
bool ScreenSpace::isReserved() const { return false; }
int ScreenSpace::arrange(void*, int, int) { return 0; }
int ScreenSpace::restore() { return 0; }
std::string ScreenSpace::statusInfo() const { return impl_->status; }
#endif

ScreenSpace::ScreenSpace() : impl_(new Impl) {}
ScreenSpace::~ScreenSpace() { release(); delete impl_; }
