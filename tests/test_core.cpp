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
#include <sessionbar/Calendars.hpp>
#include <sessionbar/Arrange.hpp>
#include <sessionbar/ReserveState.hpp>
#include <sessionbar/State.hpp>
#include <sessionbar/BarLink.hpp>

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
    CHECK(bgIsLocalChange(sync, 1, 100));
    sync.publishedStyle = 1; sync.publishedPct = 100;
    CHECK(!bgIsLocalChange(sync, 1, 100));
    CHECK(bgIsLocalChange(sync, 1, 55));  // opacity moved
    CHECK(bgIsLocalChange(sync, 2, 100)); // style moved
    // a peer publishes a new background: adopted once
    LinkShared sh;
    auto& peer = sh.slots[0]; peer.appId = 0; peer.linked = 1; peer.heartbeatMs = now;
    peer.bgSeq = 7; peer.bgStyle = 2; peer.bgOpacityPct = 40;
    BgPacket out;
    CHECK(bgAdoption(sh, 2, sync, now, out));
    CHECK(out.style == 2 && out.opacityPct == 40);
    // the adopted values are now OUR published values: no echo publish
    CHECK(!bgIsLocalChange(sync, 2, 40));
    // the same seq is never adopted twice
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    // a genuinely new seq adopts again
    peer.bgSeq = 8; peer.bgStyle = 0; peer.bgOpacityPct = 90;
    CHECK(bgAdoption(sh, 2, sync, now, out));
    CHECK(out.style == 0 && out.opacityPct == 90);
    // unlinked peers, stale peers, seq 0 and self never supply a background
    peer.linked = 0; peer.bgSeq = 9;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.linked = 1; peer.heartbeatMs = now - kPeerStaleMs - 1;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.heartbeatMs = now; peer.bgSeq = 0;
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    peer.bgSeq = 11; peer.appId = 2; // the peer slot carries OUR id: self, ignored
    CHECK(!bgAdoption(sh, 2, sync, now, out));
    // two peers changed at once: the LAST in slot order wins, both are marked seen
    LinkShared sh2; BgSyncState sync2;
    auto& p0 = sh2.slots[0]; p0.appId = 0; p0.linked = 1; p0.heartbeatMs = now; p0.bgSeq = 3; p0.bgStyle = 1; p0.bgOpacityPct = 70;
    auto& p1 = sh2.slots[1]; p1.appId = 1; p1.linked = 1; p1.heartbeatMs = now; p1.bgSeq = 4; p1.bgStyle = 2; p1.bgOpacityPct = 30;
    CHECK(bgAdoption(sh2, 2, sync2, now, out));
    CHECK(out.style == 2 && out.opacityPct == 30);
    CHECK(sync2.seenSeq[0] == 3 && sync2.seenSeq[1] == 4);
    CHECK(!bgAdoption(sh2, 2, sync2, now, out));
}

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

