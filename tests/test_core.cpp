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
#include <string>
#include <sessionbar/Sessions.hpp>
#include <sessionbar/Arrange.hpp>
#include <sessionbar/ReserveState.hpp>
#include <sessionbar/State.hpp>

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
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 13, 0)); // 08:00 EST, pre-open
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 5400 }); // opens 09:30 EST = 14:30Z
        CHECK(std::string(st.verb) == "opens");
    }
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 22, 0)); // 17:00 EST, post-close
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 59400 }); // next open Fri 14:30Z
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
        auto st = evaluate(newYorkDef(), utc(2026, 1, 15, 21, 0)); // exactly 16:00 EST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 63000 }); // Fri open is 17h30m away
    }
    // Friday close -> Monday open (the long regular gap).
    {
        auto st = evaluate(newYorkDef(), utc(2026, 1, 16, 21, 0)); // Fri 16:00 EST
        CHECK(st.state == SessionState::Closed);
        CHECK(st.secondsToChange == seconds{ 235800 }); // Mon 09:30 EST = 65h30m
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
        CHECK(st.secondsToChange == seconds{ 171000 }); // Mon 14:30Z = 47h30m
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
        auto ny = evaluate(newYorkDef(), utc(2026, 3, 10, 12, 0)); // 08:00 EDT
        CHECK(ny.state == SessionState::Closed);
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
        auto ny = evaluate(newYorkDef(), utc(2026, 10, 27, 13, 0)); // 09:00 EDT
        CHECK(ny.state == SessionState::Closed);
        CHECK(ny.secondsToChange == seconds{ 1800 }); // opens 09:30 EDT = 13:30Z
        auto ld = evaluate(londonDef(), utc(2026, 10, 27, 13, 0)); // 13:00 GMT
        CHECK(ld.state == SessionState::Open);
        CHECK(ld.secondsToChange == seconds{ 12600 }); // closes 16:30 GMT = 3h30m
    }
    // ---------- Summer sanity: both on DST (EDT / BST). Wed 2026-07-15.
    {
        auto ny = evaluate(newYorkDef(), utc(2026, 7, 15, 13, 0)); // 09:00 EDT
        CHECK(ny.state == SessionState::Closed);
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

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
