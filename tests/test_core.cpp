// SessionBar core tests — plain asserts, no framework (family idiom).
// The session cases pin REAL 2026 calendar facts:
//   US DST 2026: springs forward Sun Mar 8, falls back Sun Nov 1.
//   EU DST 2026: springs forward Sun Mar 29, falls back Sun Oct 25.
//   Japan has no DST. Winter offsets: EST = UTC-5, GMT = UTC+0,
//   JST = UTC+9. Inputs are built by pure UTC calendar arithmetic, so
//   the expected values below are independent of the tz database the
//   code under test reads — if locate_zone/zoned_time disagreed with
//   the published calendars, these checks fail.
#include <cstdio>
#include <cstdint>
#include <string>
#include <sessionbar/Sessions.hpp>
#include <sessionbar/Calendars.hpp>
#include <sessionbar/Arrange.hpp>
#include <sessionbar/ReserveState.hpp>
#include <sessionbar/State.hpp>
#include <sessionbar/BarLink.hpp>
#include <sessionbar/SemVer.hpp>
#include <sessionbar/ReleaseParse.hpp>
#include <sessionbar/WindowGeom.hpp>
#include <sessionbar/SessionColors.hpp>
#include <sessionbar/Visualizer.hpp>
#include <sessionbar/Glow.hpp>
#include <sessionbar/Mascot.hpp>
#include <sessionbar/CellLayout.hpp>
#include <sessionbar/Glyphs.hpp>
#include "SessionBarVersion.h" // v0.4.3: generated from the root project() VERSION
#include <vector>

static int failures = 0, checks = 0;
#define CHECK(cond) do{ ++checks; if(!(cond)){ ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);} else { std::printf("ok   %s\n", #cond);} }while(0)

using namespace sessionbar;
using namespace std::chrono;

static sys_seconds utc(int y, int mo, int d, int h, int mi) {
    return sys_days{ year{ y } / mo / d } + hours{ h } + minutes{ mi };
}

int main() {
    // ---------- New York, winter (EST) ----------
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 15, 0)); // Thu 10:00 EST
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 21600 }); // closes 16:00 EST = 21:00Z
        CHECK(std::string(st.verb) == "closes");
        CHECK(st.localHour == 10 && st.localMinute == 0);
        CHECK(formatLocalTime(st, true) == "10:00");
        CHECK(formatLocalTime(st, false) == "10:00 AM");
        CHECK(!st.localIsWeekend);
    }
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 13, 0)); // 08:00 EST — v0.2.0: PRE (04:00–09:30)
        CHECK(st.state == SessionState::Pre);
        CHECK(st.secondsToChange == seconds{ 5400 }); // opens 09:30 EST = 14:30Z
        CHECK(std::string(st.verb) == "opens");
        CHECK(std::string(stateName(st.state)) == "PRE");
    }
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 22, 0)); // 17:00 EST — v0.2.0: POST (16:00–20:00)
        CHECK(st.state == SessionState::Post);
        CHECK(st.secondsToChange == seconds{ 10800 }); // post ends 20:00 EST = 01:00Z
        CHECK(std::string(st.verb) == "ends");
        CHECK(std::string(stateName(st.state)) == "POST");
        CHECK(formatLocalTime(st, true) == "17:00");
        CHECK(formatLocalTime(st, false) == "5:00 PM");
    }
    // Boundary minutes: AT the open instant = OPEN, AT the close = CLOSED.
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 14, 30)); // exactly 09:30 EST
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 23400 });
    }
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 21, 0)); // exactly 16:00 EST — POST begins (half-open)
        CHECK(st.state == SessionState::Post);
        CHECK(st.secondsToChange == seconds{ 14400 }); // post ends 20:00 EST
    }
    // Friday close -> Monday open (the long regular gap).
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 16, 21, 0)); // Fri 16:00 EST — POST, not the weekend yet
        CHECK(st.state == SessionState::Post);
        CHECK(st.secondsToChange == seconds{ 14400 }); // post ends Fri 20:00 EST
    }
    // ---------- London, winter (GMT) ----------
    {
        auto st = evaluate(londonDef(), utc(2026, 1, 15, 12, 0)); // 12:00 GMT
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 16200 }); // closes 16:30 GMT
        CHECK(std::string(st.verb) == "closes");
        CHECK(st.localHour == 12);
    }
    {
        auto st = evaluate(londonDef(), utc(2026, 1, 15, 8, 0)); // exactly 08:00 GMT
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 30600 }); // full 8h30m session
    }
    // ---------- Tokyo, incl. the lunch break (JST, no DST) ----------
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 2, 0)); // 11:00 JST, morning
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 1800 }); // break at 11:30 JST
        CHECK(std::string(st.verb) == "break");
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 3, 0)); // 12:00 JST, lunch
        CHECK(st.state == SessionState::Break);
        CHECK(st.secondsToChange == seconds{ 1800 }); // resumes 12:30 JST
        CHECK(std::string(st.verb) == "resumes");
        CHECK(countdownText(st) == "resumes in 0h 30m");
        CHECK(std::string(stateName(st.state)) == "BREAK");
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 4, 0)); // 13:00 JST, afternoon
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 7200 }); // closes 15:00 JST = 06:00Z
        CHECK(std::string(st.verb) == "closes");
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 2, 30)); // exactly 11:30 JST
        CHECK(st.state == SessionState::Break);
        CHECK(st.secondsToChange == seconds{ 3600 });
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 3, 30)); // exactly 12:30 JST
        CHECK(st.state == SessionState::Open);
        CHECK(st.secondsToChange == seconds{ 9000 });
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 6, 0)); // exactly 15:00 JST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 64800 }); // Fri 09:00 JST = 00:00Z
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 15, 15, 0)); // Fri 00:00 JST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.localHour == 0 && st.localMinute == 0);
        CHECK(formatLocalTime(st, true) == "00:00");
        CHECK(formatLocalTime(st, false) == "12:00 AM");
        CHECK(st.secondsToChange == seconds{ 32400 }); // opens 09:00 JST
    }
    // ---------- Weekend rollover to Monday ----------
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 17, 15, 0)); // Sat 10:00 EST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.localIsWeekend);
        CHECK(st.secondsToChange == seconds{ 237600 }); // v0.2.0: Mon Jan 19 is MLK Day (NYSE holiday) — next is Tue PRE 04:00 EST = 66h
    }
    {
        auto st = evaluate(londonDef(), utc(2026, 1, 17, 15, 0)); // Sat 15:00 GMT
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 147600 }); // Mon 08:00Z = 41h
    }
    {
        auto st = evaluate(tokyoDef(), utc(2026, 1, 17, 15, 0)); // Sun 00:00 JST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.localIsWeekend);
        CHECK(st.secondsToChange == seconds{ 118800 }); // Mon 09:00 JST = 33h
    }
    // ---------- March mismatch week: US on DST (EDT, UTC-4) from
    // Mar 8; London still GMT until Mar 29. Tue 2026-03-10. ----------
    {
        auto ny = evaluate(newYorkDef(), utc(2026, 3, 10, 12, 0)); // 08:00 EDT — PRE
        CHECK(ny.state == SessionState::Pre);
        CHECK(ny.secondsToChange == seconds{ 5400 }); // opens 09:30 EDT = 13:30Z (NOT 14:30Z)
        auto ld = evaluate(londonDef(), utc(2026, 3, 10, 12, 0)); // 12:00 GMT
        CHECK(ld.state == SessionState::Open);
        CHECK(ld.secondsToChange == seconds{ 16200 }); // closes 16:30 GMT
    }
    {
        auto ny = evaluate(newYorkDef(), utc(2026, 3, 10, 13, 30)); // exactly 09:30 EDT
        CHECK(ny.state == SessionState::Open);
        CHECK(ny.secondsToChange == seconds{ 23400 }); // closes 16:00 EDT = 20:00Z
    }
    // ---------- Late-Oct mismatch week: London back on GMT from
    // Oct 25; New York still EDT until Nov 1. Tue 2026-10-27. ----------
    {
        auto ny = evaluate(newYorkDef(), utc(2026, 10, 27, 13, 0)); // 09:00 EDT — PRE
        CHECK(ny.state == SessionState::Pre);
        CHECK(ny.secondsToChange == seconds{ 1800 }); // opens 09:30 EDT = 13:30Z
        auto ld = evaluate(londonDef(), utc(2026, 10, 27, 13, 0)); // 13:00 GMT
        CHECK(ld.state == SessionState::Open);
        CHECK(ld.secondsToChange == seconds{ 12600 }); // closes 16:30 GMT = 3h30m
    }
    // ---------- Summer sanity: both on DST (EDT / BST). Wed 2026-07-15.
    {
        auto ny = evaluate(newYorkDef(), utc(2026, 7, 15, 13, 0)); // 09:00 EDT — PRE
        CHECK(ny.state == SessionState::Pre);
        CHECK(ny.secondsToChange == seconds{ 1800 });
        auto ld = evaluate(londonDef(), utc(2026, 7, 15, 13, 0)); // 14:00 BST
        CHECK(ld.state == SessionState::Open);
        CHECK(ld.secondsToChange == seconds{ 9000 }); // closes 16:30 BST = 15:30Z
    }
    // ---------- Formatting ----------
    CHECK(formatCountdown(seconds{ 0 }) == "0h 00m");
    CHECK(formatCountdown(seconds{ 1800 }) == "0h 30m");
    CHECK(formatCountdown(seconds{ 5400 }) == "1h 30m");
    CHECK(formatCountdown(seconds{ 59400 }) == "16h 30m");
    CHECK(formatCountdown(seconds{ 171000 }) == "47h 30m");
    CHECK(formatCountdown(seconds{ -5 }) == "0h 00m"); // clamps, never negative
    CHECK(std::string(stateName(SessionState::Open)) == "OPEN");
    CHECK(std::string(stateName(SessionState::Closed)) == "CLOSED");

    // ---------- Arrange fitting (ported family math) ----------
    {
        const IntRect mon{ 0, 0, 1920, 1080 };
        CHECK((arrangeFit(IntRect{ 100, 200, 800, 600 }, mon, 84) == IntRect{ 100, 200, 800, 600 })); // already fits
        CHECK((arrangeFit(IntRect{ 100, 40, 800, 600 }, mon, 84) == IntRect{ 100, 84, 800, 600 }));    // slides below strip
        CHECK((arrangeFit(IntRect{ 0, 0, 1920, 1080 }, mon, 84) == IntRect{ 0, 84, 1920, 996 }));      // shrinks to free area
        CHECK((arrangeFitAbove(IntRect{ 100, 100, 800, 600 }, mon, 84) == IntRect{ 100, 100, 800, 600 }));
        CHECK((arrangeFitAbove(IntRect{ 100, 900, 800, 180 }, mon, 84) == IntRect{ 100, 816, 800, 180 })); // slides above strip
        CHECK((arrangeFitAbove(IntRect{ 0, 0, 1920, 1080 }, mon, 84) == IntRect{ 0, 0, 1920, 996 }));
    }
    // ---------- Arrange: maximized windows (v0.4.6) ----------
    {
        const IntRect zoomedRect{ 0, 0, 1920, 1080 };   // a maximized window's live rect: the whole monitor
        const IntRect normalRect{ 100, 10, 800, 600 };  // its rcNormalPosition — overlaps the top strip
        // zoomed: participates, restore-first, fit input is the NORMAL rect (never the zoomed one)
        auto zp = planArrangeWindow(true, false, true, zoomedRect, normalRect);
        CHECK(zp.participate && zp.restoreFirst && zp.fitInput == normalRect);
        // ...and the fit of that normal rect slides it clear of the strip, like any window
        CHECK((arrangeFit(zp.fitInput, IntRect{ 0, 0, 1920, 1080 }, 84) == IntRect{ 100, 84, 800, 600 }));
        // normal window: participates on its live rect, no restore step
        auto np = planArrangeWindow(true, false, false, normalRect, normalRect);
        CHECK(np.participate && !np.restoreFirst && np.fitInput == normalRect);
        // the historical skips stay skips: invisible, iconic — zoomed or not
        CHECK(!planArrangeWindow(false, false, true, zoomedRect, normalRect).participate);
        CHECK(!planArrangeWindow(true, true, true, zoomedRect, normalRect).participate);
        // a zoomed window with a degenerate normal rect is left alone
        CHECK(!planArrangeWindow(true, false, true, zoomedRect, IntRect{ 0, 0, 0, 0 }).participate);
        // restore plan: a was-maximized snapshot re-maximizes after its rect is applied; others don't
        auto rp = planRestoreWindow(true);
        CHECK(rp.applyRect && rp.remaximize);
        auto rp2 = planRestoreWindow(false);
        CHECK(rp2.applyRect && !rp2.remaximize);
    }
    // ---------- Reserve state machine (MeterBar v0.18.1 discipline) ----------
    {
        ReserveMachine m;
        const IntRect top{ 0, 0, 800, 60 };
        CHECK(m.update(false, 0, 0, top, 1) == ReserveAction::None);     // off, never reserved
        CHECK(m.update(true, 0, 5, top, 1) == ReserveAction::None);      // dead zone: no dock from 5 px
        CHECK(m.update(true, 0, 0, IntRect{ 0, 0, 0, 0 }, 1) == ReserveAction::None); // degenerate rect never registers
        CHECK(m.update(true, 0, 2, top, 1) == ReserveAction::Register);  // docked at 2 px
        CHECK(m.reserved());
        CHECK(m.update(true, 0, 2, top, 1) == ReserveAction::None);      // identical repeat: silent forever
        CHECK(m.update(true, 0, 5, top, 1) == ReserveAction::None);      // dead zone holds the reservation
        CHECK(m.update(true, 0, 0, IntRect{ 10, 0, 800, 60 }, 1) == ReserveAction::Update); // geometry changed: one Update
        CHECK(m.update(true, 0, 0, IntRect{ 10, 0, 800, 60 }, 1) == ReserveAction::None);
        CHECK(m.update(true, 1, 0, IntRect{ 10, 0, 800, 60 }, 1) == ReserveAction::Update); // edge change: one Update
        CHECK(m.update(true, 1, 9, IntRect{ 10, 0, 800, 60 }, 1) == ReserveAction::Release); // past the dead zone
        CHECK(!m.reserved());
        CHECK(m.update(true, 1, 0, IntRect{ 10, 1020, 800, 60 }, 1) == ReserveAction::Register); // re-dock at bottom
        CHECK(m.update(false, 1, 0, IntRect{ 10, 1020, 800, 60 }, 1) == ReserveAction::Release); // toggle off wins
        CHECK(m.update(false, 1, 0, IntRect{ 10, 1020, 800, 60 }, 1) == ReserveAction::None);
    }
    // ---------- State defaults ----------
    {
        SessionBarState s;
        CHECK(s.showNewYork && s.showLondon && s.showTokyo);
        CHECK(!s.use24h);
        CHECK(s.themeName == "Midnight Kids");
        CHECK(s.backgroundStyle == BackgroundStyle::MidnightKids);
        CHECK(s.backgroundOpacity == 1.0f);
        CHECK(s.pinned);
        CHECK(!s.reserveScreenSpace); // reserve is strictly opt-in
        CHECK(s.dockEdge == DockEdge::Top);
    }

    // ---------- v0.2.0: exchange calendars — EVERY published date is a test ----------
    {
        auto ymd = [](int y, int m, int d) { return year_month_day{ year{ y } / m / d }; };
        CHECK(isExchangeHoliday("NY", ymd(2026, 1, 1)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 1, 19)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 2, 16)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 4, 3)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 5, 25)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 6, 19)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 7, 3)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 9, 7)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 11, 26)));
        CHECK(isExchangeHoliday("NY", ymd(2026, 12, 25)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 1, 1)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 1, 18)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 2, 15)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 3, 26)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 5, 31)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 6, 18)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 7, 5)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 9, 6)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 11, 25)));
        CHECK(isExchangeHoliday("NY", ymd(2027, 12, 24)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 1, 17)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 2, 21)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 4, 14)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 5, 29)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 6, 19)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 7, 4)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 9, 4)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 11, 23)));
        CHECK(isExchangeHoliday("NY", ymd(2028, 12, 25)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 1, 1)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 4, 3)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 4, 6)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 5, 4)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 5, 25)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 8, 31)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 12, 25)));
        CHECK(isExchangeHoliday("LDN", ymd(2026, 12, 28)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 1, 1)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 3, 26)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 3, 29)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 5, 3)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 5, 31)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 8, 30)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 12, 27)));
        CHECK(isExchangeHoliday("LDN", ymd(2027, 12, 28)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 1, 3)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 4, 14)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 4, 17)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 5, 1)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 5, 29)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 8, 28)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 12, 25)));
        CHECK(isExchangeHoliday("LDN", ymd(2028, 12, 26)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 1, 1)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 1, 2)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 1, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 1, 12)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 2, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 2, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 3, 20)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 4, 29)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 5, 4)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 5, 5)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 5, 6)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 7, 20)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 8, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 9, 21)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 9, 22)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 9, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 10, 12)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 11, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 11, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2026, 12, 31)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 1, 1)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 1, 2)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 1, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 1, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 2, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 2, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 3, 22)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 4, 29)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 5, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 5, 4)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 5, 5)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 7, 19)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 8, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 9, 20)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 9, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 10, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 11, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 11, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2027, 12, 31)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 1, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 1, 10)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 2, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 2, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 3, 20)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 5, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 5, 4)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 5, 5)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 7, 17)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 8, 11)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 9, 18)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 9, 22)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 10, 9)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 11, 3)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 11, 23)));
        CHECK(isExchangeHoliday("TYO", ymd(2028, 12, 31)));
        CHECK(earlyCloseMin("NY", ymd(2026, 11, 27)) == std::optional<int>(780));
        CHECK(earlyCloseMin("NY", ymd(2026, 12, 24)) == std::optional<int>(780));
        CHECK(earlyCloseMin("NY", ymd(2027, 11, 26)) == std::optional<int>(780));
        CHECK(earlyCloseMin("NY", ymd(2028, 7, 3)) == std::optional<int>(780));
        CHECK(earlyCloseMin("NY", ymd(2028, 11, 24)) == std::optional<int>(780));
        CHECK(earlyCloseMin("LDN", ymd(2026, 12, 24)) == std::optional<int>(750));
        CHECK(earlyCloseMin("LDN", ymd(2026, 12, 31)) == std::optional<int>(750));
        CHECK(earlyCloseMin("LDN", ymd(2027, 12, 24)) == std::optional<int>(750));
        CHECK(earlyCloseMin("LDN", ymd(2027, 12, 31)) == std::optional<int>(750));
        CHECK(earlyCloseMin("LDN", ymd(2028, 12, 22)) == std::optional<int>(750));
        CHECK(earlyCloseMin("LDN", ymd(2028, 12, 29)) == std::optional<int>(750));
        // Deliberate absences (published lists say these are TRADING days):
        CHECK(!isExchangeHoliday("NY", ymd(2028, 1, 1)));   // Jan 1 2028 is a Saturday, not observed
        CHECK(!earlyCloseMin("NY", ymd(2026, 7, 2)).has_value());  // no Jul-2 early close in 2026
        CHECK(!earlyCloseMin("NY", ymd(2027, 12, 24)).has_value()); // Christmas 2027 IS the holiday itself
        CHECK(!earlyCloseMin("TYO", ymd(2026, 12, 24)).has_value()); // Tokyo has no early closes
        CHECK(!isExchangeHoliday("NY", ymd(2026, 1, 15)));  // an ordinary Thursday
        CHECK(!isExchangeHoliday("XXX", ymd(2026, 1, 1)));  // unknown id: no calendar
    }
    // ---------- v0.2.0: calendar + PRE/POST integration ----------
    {
        auto st = evaluate(newYorkDef(), utc(2026, 11, 26, 15, 0)); // Thanksgiving, 10:00 EST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 64800 }); // Fri PRE 04:00 EST = Fri 09:00Z
        CHECK(std::string(st.verb) == "pre-market opens");
        auto pre = evaluate(newYorkDef(), utc(2026, 11, 26, 13, 0)); // Thanksgiving 08:00 EST: NO pre-market on a holiday
        CHECK(pre.state == SessionState::Closed);
        auto ec = evaluate(newYorkDef(), utc(2026, 11, 27, 17, 30)); // day after Thanksgiving, 12:30 EST
        CHECK(ec.state == SessionState::Open);
        CHECK(ec.secondsToChange == seconds{ 1800 }); // early close 13:00 EST
        CHECK(std::string(ec.verb) == "closes"); // into POST
        auto post = evaluate(newYorkDef(), utc(2026, 11, 27, 18, 30)); // 13:30 EST: post runs on after the early close
        CHECK(post.state == SessionState::Post);
        CHECK(post.secondsToChange == seconds{ 23400 }); // to 20:00 EST
        auto ld = evaluate(londonDef(), utc(2026, 12, 24, 12, 0)); // Christmas Eve, 12:00 GMT
        CHECK(ld.state == SessionState::Open);
        CHECK(ld.secondsToChange == seconds{ 1800 }); // LSE early close 12:30
        auto em = evaluate(londonDef(), utc(2026, 4, 6, 12, 0)); // Easter Monday: LSE closed
        CHECK(em.state == SessionState::Closed);
        auto ty = evaluate(tokyoDef(), utc(2026, 1, 12, 2, 0)); // Coming-of-Age Day, 11:00 JST: closed
        CHECK(ty.state == SessionState::Closed);
        auto b = evaluate(newYorkDef(), utc(2026, 1, 15, 8, 59)); // 03:59 EST: one minute before PRE
        CHECK(b.state == SessionState::Closed);
        CHECK(b.secondsToChange == seconds{ 60 });
        CHECK(std::string(b.verb) == "pre-market opens");
        auto at = evaluate(newYorkDef(), utc(2026, 1, 15, 9, 0)); // exactly 04:00 EST: PRE begins
        CHECK(at.state == SessionState::Pre);
        CHECK(at.secondsToChange == seconds{ 19800 });
        auto pe = evaluate(newYorkDef(), utc(2026, 1, 16, 1, 0)); // exactly 20:00 EST: POST over
        CHECK(pe.state == SessionState::Closed);
        CHECK(pe.secondsToChange == seconds{ 28800 }); // Fri PRE 04:00 EST
    }
// ---- BarLink protocol geometry (v0.2.0, item k) ----
{
    using namespace sessionbar;
    LinkShared sh; const long long now = 100000;
    auto& peer = sh.slots[1]; peer.appId = 1; peer.linked = 1; peer.heartbeatMs = now; peer.rect = { 100, 0, 800, 40 };
    // snap: left edge 6 px from the peer's left edge aligns exactly
    auto s1 = snapToPeers({ 106, 300, 500, 40 }, sh, 0, now);
    CHECK(s1.x == 100);
    // snap: top edge 8 px above the peer's bottom stacks flush
    auto s2 = snapToPeers({ 400, 48, 500, 40 }, sh, 0, now);
    CHECK(s2.y == 40);
    // beyond the threshold: untouched
    auto s3 = snapToPeers({ 130, 300, 500, 40 }, sh, 0, now);
    CHECK(s3.x == 130 && s3.y == 300);
    // unlinked or stale peers never snap
    peer.linked = 0; CHECK(snapToPeers({ 106, 300, 500, 40 }, sh, 0, now).x == 106);
    peer.linked = 1; peer.heartbeatMs = now - kPeerStaleMs - 1;
    CHECK(snapToPeers({ 106, 300, 500, 40 }, sh, 0, now).x == 106);
    // never snap to self
    peer.heartbeatMs = now; peer.appId = 0;
    CHECK(snapToPeers({ 106, 300, 500, 40 }, sh, 0, now).x == 106);
    // follow: pure translation, size preserved
    auto f = followRect({ 50, 500, 300, 40 }, { 0, 0, 800, 40 }, { 25, -10, 800, 40 });
    CHECK(f.x == 75 && f.y == 490 && f.w == 300 && f.h == 40);
    // group dock stacking on all four edges
    IntRect lead{ 0, 0, 800, 40 }, fol{ 200, 300, 600, 36 };
    CHECK(groupDockRect(lead, fol, 0).y == 40);
    IntRect leadB{ 0, 1040, 800, 40 };
    CHECK(groupDockRect(leadB, fol, 1).y == 1040 - 36);
    CHECK(groupDockRect(lead, fol, 2).x == 800);
    IntRect leadR{ 1120, 0, 800, 40 };
    CHECK(groupDockRect(leadR, fol, 3).x == 1120 - 600);
    // v0.2.1: the cross axis ALIGNS to the leader (field fix — a
    // follower stacked under a sideways-moved peer used to keep its
    // own x and no longer sat under the peer). Vertical stacks align
    // left edges; horizontal stacks align top edges.
    IntRect leadM{ 350, 0, 800, 40 };
    CHECK(groupDockRect(leadM, fol, 0).x == 350);
    IntRect leadMB{ 350, 1040, 800, 40 };
    auto gdB = groupDockRect(leadMB, fol, 1);
    CHECK(gdB.y == 1040 - 36 && gdB.x == 350);
    IntRect leadL{ 0, 120, 40, 800 };
    auto gdL = groupDockRect(leadL, fol, 2);
    CHECK(gdL.x == 40 && gdL.y == 120);
    IntRect leadRM{ 1880, 120, 40, 800 };
    auto gdR = groupDockRect(leadRM, fol, 3);
    CHECK(gdR.x == 1880 - 600 && gdR.y == 120);
    // size is never touched by stacking
    CHECK(gdB.w == 600 && gdB.h == 36);
    // an unknown edge ordinal leaves the follower alone
    CHECK(groupDockRect(lead, fol, -1) == fol);
    // drag liveness is staleness-based
    LinkSlot d; d.dragSeq = 3; d.dragStampMs = now;
    CHECK(dragLive(d, now)); CHECK(!dragLive(d, now + kLeaderStaleMs + 1));
}
// ---- Background sync decisions (v0.2.1, item k hardening) ----
{
    using namespace sessionbar;
    const long long now = 200000;
    BgSyncState sync;
    // first sight of our own background is a change (nothing published yet)
    CHECK(bgIsLocalChange(sync, 1, 100, 0));
    sync.publishedStyle = 1; sync.publishedPct = 100; sync.publishedEffect = 0;
    CHECK(!bgIsLocalChange(sync, 1, 100, 0));
    CHECK(bgIsLocalChange(sync, 1, 55, 0));  // opacity moved
    CHECK(bgIsLocalChange(sync, 2, 100, 0)); // style moved
    CHECK(bgIsLocalChange(sync, 1, 100, 1)); // effect moved (BLK3)
    // a peer publishes a new background: adopted once
    LinkShared sh;
    auto& peer = sh.slots[0]; peer.appId = 0; peer.linked = 1; peer.heartbeatMs = now;
    peer.bgSeq = 7; peer.bgStyle = 2; peer.bgOpacityPct = 40; peer.bgEffect = 2;
    BgPacket out;
    CHECK(bgAdoption(sh, 2, sync, now, out));
    CHECK(out.style == 2 && out.opacityPct == 40 && out.effect == 2);
    // the adopted values are now OUR published values: no echo publish
    CHECK(!bgIsLocalChange(sync, 2, 40, 2));
    // the same seq is never adopted twice
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    // a genuinely new seq adopts again
    peer.bgSeq = 8; peer.bgStyle = 0; peer.bgOpacityPct = 90; peer.bgEffect = 1;
    CHECK(bgAdoption(sh, 2, sync, now, out));
    CHECK(out.style == 0 && out.opacityPct == 90 && out.effect == 1);
    // BLK4: effect 3 (visualizer Lines) rides the same packet,
    // adopted verbatim like every other ordinal
    peer.bgSeq = 12; peer.bgEffect = 3;
    CHECK(bgAdoption(sh, 2, sync, now, out));
    CHECK(out.effect == 3);
    CHECK(!bgIsLocalChange(sync, 0, 90, 3)); // adopted = published: no echo
    // unlinked peers, stale peers, seq 0 and self never supply a background
    peer.linked = 0; peer.bgSeq = 9;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.linked = 1; peer.heartbeatMs = now - kPeerStaleMs - 1;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.heartbeatMs = now; peer.bgSeq = 0;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.bgSeq = 11; peer.appId = 2; // the peer slot carries OUR id: self, ignored
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    // v0.4.14 parent ownership: two peers changed at once — the
    // PARENT (lowest appId) supplies the background; the follower
    // peer's packet is not consumed at all.
    LinkShared sh2; BgSyncState sync2;
    auto& p0 = sh2.slots[0]; p0.appId = 0; p0.linked = 1; p0.heartbeatMs = now; p0.bgSeq = 3; p0.bgStyle = 1; p0.bgOpacityPct = 70;
    auto& p1 = sh2.slots[1]; p1.appId = 1; p1.linked = 1; p1.heartbeatMs = now; p1.bgSeq = 4; p1.bgStyle = 2; p1.bgOpacityPct = 30;
    CHECK(bgAdoption(sh2, 2, sync2, now, out));
    CHECK(out.style == 1 && out.opacityPct == 70);
    CHECK(sync2.seenSeq[0] == 3 && sync2.seenSeq[1] == 0);
    CHECK(sync2.adoptedFrom == 0);
    CHECK(!bgAdoption(sh2, 2, sync2, now, out));
    // the parent itself adopts from NOBODY, however fresh the
    // followers' packets are (the pre-v0.4.14 swap defect: at link
    // join each bar adopted the other's stale background)
    BgSyncState syncP;
    CHECK(!bgAdoption(sh2, 0, syncP, now, out));
    CHECK(syncP.adoptedFrom == -1);
    // with the parent gone, the next-lowest member becomes the
    // parent: its standing packet is adopted even though it was
    // never consumed before (parent turnover)
    p0.linked = 0;
    CHECK(bgAdoption(sh2, 2, sync2, now, out));
    CHECK(out.style == 2 && out.opacityPct == 30);
    CHECK(sync2.adoptedFrom == 1);
    // and when the original parent returns with the SAME seq it had
    // before, its packet is authoritative again — adoptedFrom, not
    // the seen marker, decides (the values changed hands meanwhile)
    p0.linked = 1;
    CHECK(bgAdoption(sh2, 2, sync2, now, out));
    CHECK(out.style == 1 && out.opacityPct == 70);
    CHECK(sync2.adoptedFrom == 0);
    CHECK(!bgAdoption(sh2, 2, sync2, now, out));
}
// ---- BLK6: first-clicked-wins parentage + member-drag leadership ----
{
    using namespace sessionbar;
    const long long now = 300000;
    BgPacket out;
    // FIRST-CLICKED WINS: appId 2 claimed linkSeq 1 (clicked first),
    // appId 0 claimed 3 — the parent is 2, NOT the lowest appId (the
    // v0.25.1 fixed-slot rule Jackson's 2026-11-01 field test rejected).
    LinkShared sh;
    auto& s0 = sh.slots[0]; s0.appId = 0; s0.linked = 1; s0.heartbeatMs = now; s0.linkSeq = 3; s0.bgSeq = 4; s0.bgStyle = 1; s0.bgOpacityPct = 70;
    auto& s1 = sh.slots[1]; s1.appId = 1; s1.linked = 1; s1.heartbeatMs = now; s1.linkSeq = 2; s1.bgSeq = 5; s1.bgStyle = 0; s1.bgOpacityPct = 90;
    auto& s2 = sh.slots[2]; s2.appId = 2; s2.linked = 1; s2.heartbeatMs = now; s2.linkSeq = 1; s2.bgSeq = 6; s2.bgStyle = 2; s2.bgOpacityPct = 30;
    CHECK(bgParentApp(sh, 1, 2, now) == 2);
    BgSyncState sy1;
    CHECK(bgAdoption(sh, 1, sy1, now, out));
    CHECK(out.style == 2 && out.opacityPct == 30 && sy1.adoptedFrom == 2);
    CHECK(sy1.seenSeq[0] == 0); // the lowest-appId non-parent packet is never consumed
    // the first-clicked bar itself adopts from nobody
    BgSyncState sy2;
    CHECK(!bgAdoption(sh, 2, sy2, now, out));
    // the parent's SLOT is resolved by appId after selection: with
    // self's own slot sitting between the parent's and a follower's,
    // adoption must still read the PARENT's packet (a first draft of
    // bgParentApp returned self's slot index here and adopted self).
    {
        LinkShared shS; BgSyncState syS;
        auto& q0 = shS.slots[0]; q0.appId = 0; q0.linked = 1; q0.heartbeatMs = now; q0.linkSeq = 1; q0.bgSeq = 21; q0.bgStyle = 3; q0.bgOpacityPct = 25;
        auto& q1 = shS.slots[1]; q1.appId = 1; q1.linked = 1; q1.heartbeatMs = now; q1.linkSeq = 0; q1.bgSeq = 22; // self unclaimed: the geometry that exposed the slot bug q1.bgStyle = 0; q1.bgOpacityPct = 99;
        auto& q2 = shS.slots[2]; q2.appId = 2; q2.linked = 1; q2.heartbeatMs = now; q2.linkSeq = 3; q2.bgSeq = 23; q2.bgStyle = 1; q2.bgOpacityPct = 55;
        CHECK(bgAdoption(shS, 1, syS, now, out));
        CHECK(out.style == 3 && out.opacityPct == 25 && syS.adoptedFrom == 0);
        CHECK(syS.seenSeq[0] == 21 && syS.seenSeq[1] == 0 && syS.seenSeq[2] == 0);
    }
    // LATE JOINER NEVER PARENT: appId 0 re-claims with the largest
    // linkSeq and a fresh packet — the group still follows appId 2,
    // and the joiner adopts the parent's packet from its own seat.
    LinkShared shJ = sh; shJ.slots[0].linkSeq = 9; shJ.slots[0].bgSeq = 40;
    BgSyncState syJ;
    CHECK(bgAdoption(shJ, 0, syJ, now, out));
    CHECK(syJ.adoptedFrom == 2 && out.opacityPct == 30);
    // PARENT PACKET ARRIVES AFTER THE FOLLOWER'S FIRST SCAN: the
    // follower scanned while the first-clicked bar's slot was not yet
    // in the block (its app started later); when the parent's packet
    // appears, adoption fires on that tick.
    LinkShared shL; BgSyncState syL;
    auto& l1 = shL.slots[1]; l1.appId = 1; l1.linked = 1; l1.heartbeatMs = now; l1.linkSeq = 2; l1.bgSeq = 3; l1.bgStyle = 0; l1.bgOpacityPct = 90;
    CHECK(!bgAdoption(shL, 1, syL, now, out)); // alone in the block: self is parent
    auto& l0 = shL.slots[0]; l0.appId = 0; l0.linked = 1; l0.heartbeatMs = now; l0.linkSeq = 1; l0.bgSeq = 2; l0.bgStyle = 3; l0.bgOpacityPct = 15;
    CHECK(bgAdoption(shL, 1, syL, now, out));
    CHECK(out.style == 3 && out.opacityPct == 15 && syL.adoptedFrom == 0);
    // HANDOFF: the parent unlinks — the earliest-linked REMAINING
    // member becomes parent, and its standing packet is adopted even
    // though that bgSeq was already seen (identity change re-fires).
    LinkShared shH; BgSyncState syH;
    auto& h0 = shH.slots[0]; h0.appId = 0; h0.linked = 1; h0.heartbeatMs = now; h0.linkSeq = 1; h0.bgSeq = 7; h0.bgStyle = 1; h0.bgOpacityPct = 60;
    auto& h1 = shH.slots[1]; h1.appId = 1; h1.linked = 1; h1.heartbeatMs = now; h1.linkSeq = 2; h1.bgSeq = 8; h1.bgStyle = 2; h1.bgOpacityPct = 45;
    auto& h2 = shH.slots[2]; h2.appId = 2; h2.linked = 1; h2.heartbeatMs = now; h2.linkSeq = 3; h2.bgSeq = 9; h2.bgStyle = 0; h2.bgOpacityPct = 95;
    CHECK(bgAdoption(shH, 2, syH, now, out)); CHECK(syH.adoptedFrom == 0);
    syH.seenSeq[1] = 8; // h1's packet was consumed under the old regime
    h0.linked = 0;
    CHECK(bgParentApp(shH, 2, 3, now) == 1);
    CHECK(bgAdoption(shH, 2, syH, now, out));
    CHECK(out.style == 2 && out.opacityPct == 45 && syH.adoptedFrom == 1);
    // A RELINKED PARENT RETURNS AS A LATE JOINER: it claims a fresh
    // (largest) linkSeq, so it now ADOPTS from the standing parent.
    h0.linked = 1; h0.linkSeq = 10;
    BgSyncState syH0;
    CHECK(bgAdoption(shH, 0, syH0, now, out));
    CHECK(syH0.adoptedFrom == 1 && out.opacityPct == 45);
    // MEMBER-DRAG LEADERSHIP: whichever member leads a drag session,
    // every other member's follow rect translates by the leader's
    // delta with size preserved — the pure half of Jackson's rule
    // "dragging ANY linked bar moves the whole stack" (the client
    // half — every bar's geometry feed stamps a session — is pinned
    // by the family wiring — every bar's geometry feed stamps a session).
    for (int leader = 0; leader < 3; ++leader) {
        LinkShared shD;
        for (int a = 0; a < 3; ++a) {
            auto& sl = shD.slots[a]; sl.appId = a; sl.linked = 1; sl.heartbeatMs = now;
            sl.rect = IntRect{ 100 + a * 700, 200, 600, 40 };
        }
        auto& ld = shD.slots[leader];
        ld.dragSeq = 5; ld.dragStart = ld.rect; ld.dragStampMs = now;
        ld.rect = IntRect{ ld.rect.x + 30, ld.rect.y - 12, ld.rect.w, ld.rect.h };
        CHECK(dragLive(ld, now));
        for (int a = 0; a < 3; ++a) {
            if (a == leader) continue;
            auto fr = followRect(shD.slots[a].rect, ld.dragStart, ld.rect);
            CHECK(fr.x == shD.slots[a].rect.x + 30 && fr.y == shD.slots[a].rect.y - 12);
            CHECK(fr.w == 600 && fr.h == 40);
        }
    }
}


    // ---------- v0.3.0: SemVer + ReleaseParse ----------
    {
        SemVer v;
        CHECK(parseSemVer("v0.3.0", v) && v.minor == 3 && v.patch == 0);
        CHECK(semVerIsNewer("v0.3.0", "0.2.1"));
        CHECK(!semVerIsNewer("v0.2.1", "0.3.0"));
        ReleaseFields f;
        CHECK(parseReleaseJson(R"({"tag_name":"v0.3.0","body":"n","assets":[{"name":"SessionBar-Setup.exe","browser_download_url":"https://example.invalid/SessionBar-Setup.exe","size":42,"digest":"sha256:beef"}]})", f));
        CHECK(f.hasAsset && f.assetSize == 42 && f.assetDigest == "sha256:beef");
        ReleaseFields g;
        CHECK(parseReleaseJson(R"({"tag_name":"v0.3.0","assets":[{"name":"Other.exe","size":1}]})", g));
        CHECK(!g.hasAsset); // only the exact installer name counts
    }
    // ---------- v0.3.0: WindowGeom clamp (the off-screen restore bug) ----------
    {
        std::vector<IntRect> areas{ IntRect{0, 0, 1920, 1040} };
        IntRect inside{ 100, 100, 1060, 108 };
        CHECK(clampRectToWorkAreas(inside, areas) == inside);
        // the field case: a rect saved entirely below/right of every monitor
        IntRect lost{ 2400, 1500, 1060, 108 };
        auto c = clampRectToWorkAreas(lost, areas);
        CHECK(c.x == 1920 - 1060 && c.y == 1040 - 108 && c.w == 1060 && c.h == 108);
        // partially under the taskbar at the bottom edge
        IntRect sunk{ 100, 1030, 1060, 108 };
        auto c2 = clampRectToWorkAreas(sunk, areas);
        CHECK(c2.y == 1040 - 108);
        // negative coordinates (dragged off the top-left) come back too
        auto c3 = clampRectToWorkAreas(IntRect{ -900, -200, 1060, 108 }, areas);
        CHECK(c3.x == 0 && c3.y == 0);
        CHECK(clampRectToWorkAreas(inside, {}) == inside);
    }
    // ---------- v0.4.2: WindowGeom visibility floor (link applies) ----------
    {
        std::vector<IntRect> areas{ IntRect{0, 0, 1920, 1040} };
        std::vector<IntRect> two{ IntRect{0, 0, 1920, 1040}, IntRect{1920, 0, 1920, 1040} };
        // fully off-screen: the floor fires — size kept, primary area,
        // centred horizontally, near the top
        IntRect lost{ 5000, 3000, 1060, 108 };
        CHECK(!rectMeaningfullyVisible(lost, areas));
        auto f = ensureRectVisible(lost, areas);
        CHECK(f.x == (1920 - 1060) / 2 && f.y == 24 && f.w == 1060 && f.h == 108);
        CHECK(rectMeaningfullyVisible(f, areas));
        // genuinely on the second display: untouched
        IntRect onSecond{ 2000, 50, 1060, 108 };
        CHECK(rectMeaningfullyVisible(onSecond, two));
        CHECK(ensureRectVisible(onSecond, two) == onSecond);
        // partially visible ABOVE the floor (120x108 showing): untouched
        IntRect partial{ 1800, 100, 1060, 108 };
        CHECK(rectMeaningfullyVisible(partial, areas));
        CHECK(ensureRectVisible(partial, areas) == partial);
        // a 20 px sliver is below the floor: rescued to the primary
        IntRect sliver{ 1900, 100, 1060, 108 };
        CHECK(!rectMeaningfullyVisible(sliver, areas));
        CHECK(ensureRectVisible(sliver, areas).x == (1920 - 1060) / 2);
        // sunk so only 10 px of height shows: below the floor too
        CHECK(!rectMeaningfullyVisible(IntRect{ 100, 1030, 1060, 108 }, areas));
        // straddling two monitors with a real chunk on each: untouched
        IntRect straddle{ 1700, 100, 1060, 108 };
        CHECK(ensureRectVisible(straddle, two) == straddle);
        // no areas = no opinion
        CHECK(ensureRectVisible(lost, {}) == lost);
    }
    // ---------- v0.3.0: SessionColors ----------
    {
        CHECK(sessionStateColour(0, SessionState::Open) == Rgb({47, 214, 138}));   // Family green, verbatim
        CHECK(sessionStateColour(0, SessionState::Closed) == Rgb({254, 90, 60}));  // Family red, verbatim
        CHECK(sessionStateColour(1, SessionState::Open) == Rgb({240, 249, 33}));   // Plasma 1.0 stop
        for (int sc = 0; sc < kSessionColorSchemeCount; ++sc) {
            auto open = sessionStateColour(sc, SessionState::Open);
            auto brk = sessionStateColour(sc, SessionState::Break);
            auto pre = sessionStateColour(sc, SessionState::Pre);
            auto cls = sessionStateColour(sc, SessionState::Closed);
            auto post = sessionStateColour(sc, SessionState::Post);
            CHECK(open != cls && open != brk && cls != post); // states stay distinguishable
            CHECK(brk == pre);                               // the family's Pre=Break language
        }
        CHECK(std::string(sessionColorSchemeName(3)) == "Viridis");
    }
    // ---------- v0.4.13: distributed emitters + Field style + BLK5 ----------
    {
        VisualizerSettings vs;
        // burst birthplaces follow the band's field position: low
        // left, mid centre, high right (the centre is a coordinate
        // reference, not a spawn point — Jackson, 2026-11-01)
        auto burstMeanX = [&](int band, float balance, int frames) {
            VisualizerEngine e2;
            float hot[3] = { 0, 0, 0 }; hot[(size_t)band] = 0.95f;
            for (int i = 0; i < frames; ++i) e2.update(0.016f, hot, vs, balance);
            double sx = 0; int n = 0;
            for (auto& p : e2.particles()) if (p.burst) { sx += p.x; ++n; }
            CHECK(n > 0);
            return n ? sx / n : 0.5;
        };
        const double lowX = burstMeanX(0, 0.f, 1), midX = burstMeanX(1, 0.f, 1), highX = burstMeanX(2, 0.f, 1);
        CHECK(lowX < 0.40);
        CHECK(midX > 0.38 && midX < 0.62);
        CHECK(highX > 0.60);
        CHECK(lowX < midX && midX < highX);
        // stereo balance shifts the same band's births sideways
        CHECK(burstMeanX(1, 0.9f, 20) > burstMeanX(1, -0.9f, 20) + 0.05);
        // the mapping itself: ordered bands, balance clamped in-field
        CHECK(vizBandFieldX(0, 0.f) < vizBandFieldX(1, 0.f) && vizBandFieldX(1, 0.f) < vizBandFieldX(2, 0.f));
        CHECK(vizBandFieldX(2, 1.f) <= 0.94f && vizBandFieldX(0, -1.f) >= 0.06f);
        // Field style: population fills to its target and never
        // exceeds it; the gate opens with music; vortices wander
        VisualizerField fld;
        const float loud[3] = { 0.8f, 0.6f, 0.4f };
        for (int i = 0; i < 120; ++i) { fld.update(1.f / 30.f, loud, vs); CHECK((int)fld.particles().size() <= VisualizerField::kFieldTarget); }
        CHECK((int)fld.particles().size() == VisualizerField::kFieldTarget);
        CHECK(fld.visibility() == 1.f);
        float wx0, wy0, wx1, wy1;
        fld.vortexCentre(0, wx0, wy0);
        for (int i = 0; i < 600; ++i) fld.update(1.f / 30.f, loud, vs); // +20 s
        fld.vortexCentre(0, wx1, wy1);
        CHECK(std::hypot(wx1 - wx0, wy1 - wy0) > 0.05);
        // silence: the gate closes completely, like the other styles
        VisualizerField quiet;
        const float sil[3] = { 0, 0, 0 };
        for (int i = 0; i < 600; ++i) quiet.update(1.f / 30.f, sil, vs);
        CHECK(quiet.visibility() == 0.f);
        // BLK5: the magic moved with the widened domain, and the
        // stereo balance rides the audio packet at [4]
        CHECK(kLinkMagic == 0x424C4B36u); // BLK6
        LinkSlot slot; slot.appId = 1; slot.heartbeatMs = 1000; slot.audioSeq = 3;
        slot.audioLow = 0.1f; slot.audioMid = 0.2f; slot.audioHigh = 0.3f; slot.audioLevel = 0.4f; slot.audioBalance = 0.7f;
        float pkt[5] = { 0, 0, 0, 0, 0 };
        CHECK(audioPacket(slot, 1000, pkt));
        CHECK(pkt[0] == 0.1f && pkt[3] == 0.4f && pkt[4] == 0.7f);
    }
    // ---------- v0.3.0: GlowField + emitter visualizer ----------
    {
        GlowSettings gs;
        GlowField f;
        const float zero[3] = { 0.f, 0.f, 0.f };
        f.update(0.05f, zero, 0.f);
        auto base = f.sample(0.5f, 0.5f, gs);
        CHECK(base.r <= 10 && base.b <= 14); // near-black at rest
        const float hot[3] = { 1.f, 0.5f, 0.5f };
        for (int i = 0; i < 40; ++i) f.update(0.05f, hot, 0.4f);
        auto lit = f.sample(0.22f, 0.58f, gs);
        CHECK(lit.r > 40 || lit.b > 40);
        VisualizerSettings vs; vs.enabled = true;
        VisualizerEngine e;
        const float loud[3] = { 0.2f, 0.95f, 0.2f };
        e.update(0.016f, loud, vs);
        int heads = 0; for (auto& p : e.particles()) if (p.emitter) ++heads;
        CHECK(heads >= 3);
        for (int i = 0; i < 30; ++i) e.update(0.016f, loud, vs);
        int dots = 0; for (auto& p : e.particles()) if (p.burst && !p.emitter) ++dots;
        CHECK(dots > 0); // strand shedding happened
    }
    // ---------- v0.4.0: silence gate — no white particles in silence (family feel pass, ported) ----------
    {
        VisualizerSettings vs; vs.enabled = true;
        VisualizerEngine e;
        const float zero[3] = { 0.f, 0.f, 0.f };
        for (int i = 0; i < 240; ++i) e.update(0.016f, zero, vs); // ~4 s of silence
        CHECK(e.activity() < 0.02f);
        CHECK(e.ambientVisibility() < 0.05f);
        int brightest = 0;
        for (auto& p : e.particles()) if (!p.burst) {
            auto c = e.colorFor(p, vs);
            if ((int)c.r > brightest) brightest = c.r;
        }
        CHECK(brightest < 12); // ambient whites are (near-)invisible in silence
        const float mid[3] = { 0.5f, 0.5f, 0.5f }; // real energy, below the burst trigger
        for (int i = 0; i < 90; ++i) e.update(0.016f, mid, vs);
        CHECK(e.activity() > 0.3f);
        CHECK(e.ambientVisibility() > 0.9f);
        brightest = 0;
        for (auto& p : e.particles()) if (!p.burst) {
            auto c = e.colorFor(p, vs);
            if ((int)c.r > brightest) brightest = c.r;
        }
        // v0.4.7 retune: the intent is unchanged (whites clearly
        // back once the bands carry energy) but the scale is the
        // calmed peak now — kAmbientPeak x 255 = 140, not the old
        // full-brightness 255 this check was written against.
        CHECK(brightest > 100);
        CHECK(brightest <= (int)std::lround(255.f * VisualizerEngine::kAmbientPeak) + 1);
    }
    // ---------- v0.4.7: calmed whites + Lines style ----------
    {
        VisualizerSettings vs; vs.enabled = true;
        VisualizerEngine e;
        const float loudMid[3] = { 0, 0.9f, 0 };
        for (int i = 0; i < 90; ++i) e.update(1.f / 30.f, loudMid, vs); // 3 s of music: gate fully open
        CHECK(e.ambientVisibility() == 1.f);
        int peak = 0; bool sawAmbient = false;
        for (auto& p : e.particles()) {
            if (p.burst) continue;
            sawAmbient = true;
            auto c = e.colorFor(p, vs);
            if ((int)c.r > peak) peak = (int)c.r;
        }
        CHECK(sawAmbient);
        CHECK(peak <= (int)std::lround(255.f * VisualizerEngine::kAmbientPeak) + 1);
        CHECK(peak > 60); // still present, not deleted
        // whites fade OUT at end of life, like the burst dots:
        VizParticle mid_; mid_.burst = false; mid_.age = 2.f; mid_.life = 4.f;
        VizParticle old; old.burst = false; old.age = 3.8f; old.life = 4.f;
        CHECK(e.colorFor(old, vs).r < e.colorFor(mid_, vs).r);

        // Lines: silence = still, thin, invisible
        VisualizerLines lines;
        const float silence[3] = { 0, 0, 0 };
        for (int i = 0; i < 60; ++i) lines.update(1.f / 30.f, silence);
        CHECK(lines.visibility() == 0.f);
        CHECK(lines.level(1) == 0.f);
        CHECK(lines.halfThick(1, 24) <= 0.0045f); // hairline at rest (0.004 x breathe <= 1)
        bool inRange = true;
        for (int b = 0; b < 3; ++b)
            for (int i = 0; i < VisualizerLines::kPoints; ++i) {
                float y = lines.centreY(b, i);
                if (!(y > 0.f && y < 1.f)) inRange = false;
            }
        CHECK(inRange);
        // a loud band swells its ribbon
        const float loudLow[3] = { 0.95f, 0, 0 };
        const float thickRest = lines.halfThick(0, 24);
        for (int i = 0; i < 60; ++i) lines.update(1.f / 30.f, loudLow);
        CHECK(lines.level(0) > 0.5f);
        CHECK(lines.level(1) == 0.f);
        CHECK(lines.halfThick(0, 24) > thickRest * 2.f);
        CHECK(lines.visibility() == 1.f);
        // spring, not a copy: one frame must NOT slam to target
        VisualizerLines one;
        one.update(1.f / 30.f, loudLow);
        CHECK(one.level(0) < 0.2f);
        for (int i = 0; i < 30; ++i) lines.update(1.f / 30.f, loudLow);
        inRange = true;
        for (int b = 0; b < 3; ++b)
            for (int i = 0; i < VisualizerLines::kPoints; ++i) {
                float y = lines.centreY(b, i);
                if (!(y > 0.f && y < 1.f)) inRange = false;
            }
        CHECK(inRange);
    }
    // ---------- v0.3.0: BarLink stack planner + audio + republish ----------
    {
        const long long now = 50000;
        LinkShared sh;
        auto& s0 = sh.slots[2]; s0.appId = 2; s0.linked = 1; s0.heartbeatMs = now; s0.rect = IntRect{0, 0, 1060, 108};
        s0.stackEdge = 0; s0.stackAnchor = s0.rect;
        auto& t1 = sh.slots[1]; t1.appId = 1; t1.linked = 1; t1.heartbeatMs = now; t1.rect = IntRect{300, 400, 900, 96};
        auto& m0 = sh.slots[0]; m0.appId = 0; m0.linked = 1; m0.heartbeatMs = now; m0.rect = IntRect{9, 9, 800, 40};
        IntRect targets[3];
        CHECK(planStackAll(sh, now, 2, targets) == 3);
        CHECK(targets[2] == s0.rect);
        CHECK(targets[0].y == 108 && targets[0].x == 0);        // Meter first under the anchor (family order)
        CHECK(targets[1].y == 108 + 40 && targets[1].x == 0);   // then Ticker
        float pkt[4];
        CHECK(!audioPacket(m0, now, pkt));
        m0.audioSeq = 1; m0.audioLevel = 0.5f;
        CHECK(audioPacket(m0, now, pkt) && pkt[3] == 0.5f);
        BgSyncState bs; bs.publishedStyle = 1; bs.publishedPct = 55; bs.publishedEffect = 0;
        bgForceRepublish(bs);
        CHECK(bgIsLocalChange(bs, 1, 55, 0));
    }
    // ---------- group field map: one visualizer field across the linked bars ----------
    {
        // Unlinked / alone: group == own -> the historical bar-local map.
        IntRect own{ 100, 200, 800, 40 };
        auto m0 = fieldMap(own, own, 400, 20);
        CHECK(!m0.grouped);
        float lx = -1, ly = -1;
        fieldToLocal(m0, 0.5f, 0.5f, lx, ly);
        CHECK(lx == 200.f && ly == 10.f);
        fieldToLocal(m0, 0.f, 0.f, lx, ly);
        CHECK(lx == 0.f && ly == 0.f);
        // Degenerate rects fall back to the local map, never a wild scale.
        auto mD = fieldMap(own, IntRect{}, 400, 20);
        CHECK(!mD.grouped && mD.kx == 400.f && mD.ky == 20.f);
        // Two bars side by side: the field centre is the seam — it maps
        // to the left bar's right cache edge and the right bar's left
        // cache edge (the same screen point from both sides).
        IntRect grp{ 0, 0, 1600, 40 };
        IntRect left{ 0, 0, 800, 40 }, right{ 800, 0, 800, 40 };
        auto mL = fieldMap(left, grp, 400, 20);
        auto mR = fieldMap(right, grp, 400, 20);
        CHECK(mL.grouped && mR.grouped);
        fieldToLocal(mL, 0.5f, 0.5f, lx, ly); CHECK(lx == 400.f);
        fieldToLocal(mR, 0.5f, 0.5f, lx, ly); CHECK(lx == 0.f);
        fieldToLocal(mL, 0.25f, 0.5f, lx, ly); CHECK(lx == 200.f); // quarter point: centre of the left bar
        fieldToLocal(mR, 0.75f, 0.5f, lx, ly); CHECK(lx == 200.f); // three-quarter: centre of the right bar
        // Three bars: the middle bar's cache spans the middle third.
        IntRect grp3{ 0, 0, 2400, 40 };
        IntRect mid3{ 800, 0, 800, 40 };
        auto mM = fieldMap(mid3, grp3, 400, 20);
        fieldToLocal(mM, 0.5f, 0.5f, lx, ly); CHECK(lx == 200.f);
        fieldToLocal(mM, 1.f / 3.f, 0.5f, lx, ly); CHECK(lx > -0.01f && lx < 0.01f);
        // Vertical stack: the same on the y axis.
        IntRect vgrp{ 0, 0, 800, 80 };
        IntRect top{ 0, 0, 800, 40 }, bot{ 0, 40, 800, 40 };
        auto mT = fieldMap(top, vgrp, 400, 20);
        auto mB = fieldMap(bot, vgrp, 400, 20);
        fieldToLocal(mT, 0.5f, 0.5f, lx, ly); CHECK(ly == 20.f);
        fieldToLocal(mB, 0.5f, 0.5f, lx, ly); CHECK(ly == 0.f);
        // Visibility culling: a dot deep in the left slice is invisible
        // to the right bar; a dot straddling an edge still draws.
        CHECK(fieldDotVisible(200.f, 10.f, 3.f, 400, 20));
        CHECK(!fieldDotVisible(-60.f, 10.f, 3.f, 400, 20));
        CHECK(!fieldDotVisible(460.f, 10.f, 3.f, 400, 20));
        CHECK(fieldDotVisible(-2.f, 10.f, 3.f, 400, 20));
    }
    // ---------- v0.3.0: cell layout math ----------
    {
        auto w = solveCellWidths(1060, 8, {0, 0, 0}, 120);
        CHECK(w[0] + w[1] + w[2] == 1060 - 16);
        auto w2 = solveCellWidths(1060, 8, {400, 0, 0}, 120);
        CHECK(w2[0] == 400 && w2[1] == w2[2]);
        auto pr = adjustPairWidths(300, 300, -120, 100);
        CHECK(pr.first == 180 && pr.second == 420);
        CHECK(dropSlot({200, 500, 800}, 600) == 2);
        CHECK(indexOrderAfterMove(3, 2, 0) == std::vector<int>({2, 0, 1}));
    }
    // ---------- v0.3.0: Mascot envelope + state defaults ----------
    {
        MascotAnim ma;
        ma.triggerBob(500);
        CHECK(ma.bobPx(565) < -4.9f && ma.bobPx(700) == 0.f);
        ma.triggerExcited(1000, 3000);
        CHECK(ma.excited(3999) && !ma.excited(4001));
        SessionBarState d;
        CHECK(d.cellOrder == std::vector<std::string>({"NY", "LDN", "TYO"}));
        CHECK(!d.clickSounds && d.bgEffect == 0 && d.stateColorScheme == 0 && d.uiVer == 3);
        CHECK((int)BackgroundStyle::Pastel == 3);
        CHECK((int)DockEdge::Left == 2 && (int)DockEdge::Right == 3);
        CHECK(d.windowH == 108);
    }

    // ---------- v0.4.3: single-source version (generated header) ----------
    {
        // sessionbar::kVersionString is the exact string the updater
        // compares against and getApplicationVersion() reports; it is
        // generated from the root project() VERSION. If it ever
        // disagrees with the project version, the build is lying
        // about its own version (the stuck-on-0.4.0 class: hard-coded
        // literals in Main.cpp / MainComponent.cpp, never bumped —
        // here they froze at 0.3.0).
        CHECK(std::string(sessionbar::kVersionString) == EXPECTED_PROJECT_VERSION);
        CHECK(std::string(SESSIONBAR_VERSION_STRING) == EXPECTED_PROJECT_VERSION);
        const std::string composed = std::to_string(SESSIONBAR_VERSION_MAJOR) + "."
                                   + std::to_string(SESSIONBAR_VERSION_MINOR) + "."
                                   + std::to_string(SESSIONBAR_VERSION_PATCH);
        CHECK(composed == std::string(sessionbar::kVersionString));
    }

    // ---- Glyphs (v0.4.10): the toolbar icon glyphs must be
    // EXACTLY the UTF-8 encoding of their codepoints. The family
    // field failure was mojibake — MSVC decoded the raw literals
    // as Windows-1252, so each sequence's first byte surfaced as
    // a Latin-1 char (0xF0, 0xE2). Pin every glyph two
    // ways: against an independent codepoint->UTF-8 encoder
    // written here, and against the byte sequences themselves.
    {
        auto encodeUtf8 = [](std::uint32_t cp) {
            std::string out;
            if (cp < 0x80) {
                out += (char)cp;
            } else if (cp < 0x800) {
                out += (char)(0xC0 | (cp >> 6));
                out += (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                out += (char)(0xE0 | (cp >> 12));
                out += (char)(0x80 | ((cp >> 6) & 0x3F));
                out += (char)(0x80 | (cp & 0x3F));
            } else {
                out += (char)(0xF0 | (cp >> 18));
                out += (char)(0x80 | ((cp >> 12) & 0x3F));
                out += (char)(0x80 | ((cp >> 6) & 0x3F));
                out += (char)(0x80 | (cp & 0x3F));
            }
            return out;
        };
        CHECK(std::string(sessionbar::glyphLinkUtf8()) == encodeUtf8(0x1F517));
        CHECK(std::string(sessionbar::glyphLinkUtf8()) == std::string("\xF0\x9F\x94\x97", 4));
        CHECK(std::string(sessionbar::glyphSettingsUtf8()) == encodeUtf8(0x2699));
        CHECK(std::string(sessionbar::glyphSettingsUtf8()) == std::string("\xE2\x9A\x99", 3));
        CHECK(std::string(sessionbar::glyphMenuUtf8()) == encodeUtf8(0x2630));
        CHECK(std::string(sessionbar::glyphMenuUtf8()) == std::string("\xE2\x98\xB0", 3));
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

