#pragma once
#include <chrono>
#include <string>
namespace sessionbar {
// SESSION MATH — the whole product, as pure functions of an injected
// "now". No OS calls, no globals, no caching: evaluate() takes a
// sys_seconds and returns the session's full status, so the test suite
// grades every interesting instant (DST weeks, boundaries, weekends)
// exactly, on any machine.
//
// DST DISCIPLINE (the reason this file exists): session hours are
// defined in each exchange's LOCAL wall-clock time and resolved through
// the real IANA tz database (std::chrono::locate_zone / zoned_time).
// Open/close instants are built as local times and converted — never
// computed from a fixed UTC offset. The US/EU mismatch weeks (US
// springs forward ~3 weeks before the EU in March; the EU falls back
// one week before the US in late Oct) are exactly what the tests pin.
//
// Schedule model: Mon–Fri trading days (exchange HOLIDAYS ARE NOT
// MODELED in v0.1 — the README says so plainly). A day is a list of
// half-open segments [begin, end): one Open segment, or for Tokyo
// Open / Break / Open around the lunch break. Half-open ends make the
// boundary minutes exact: AT the open instant the session is OPEN, AT
// the close instant it is CLOSED.

enum class SessionState { Open, Break, Closed };

struct SessionDef {
    const char* id;    // short id, used in settings ("NY")
    const char* city;  // display name ("NEW YORK")
    const char* tzName; // IANA zone the schedule lives in
    int openMin;       // local minutes from midnight
    int closeMin;
    int breakStartMin; // -1 = no midday break
    int breakEndMin;
};

inline const SessionDef& newYorkDef() {
    static const SessionDef d{ "NY", "NEW YORK", "America/New_York", 9 * 60 + 30, 16 * 60, -1, -1 };
    return d;
}
inline const SessionDef& londonDef() {
    static const SessionDef d{ "LDN", "LONDON", "Europe/London", 8 * 60, 16 * 60 + 30, -1, -1 };
    return d;
}
inline const SessionDef& tokyoDef() {
    static const SessionDef d{ "TYO", "TOKYO", "Asia/Tokyo", 9 * 60, 15 * 60, 11 * 60 + 30, 12 * 60 + 30 };
    return d;
}

struct SessionStatus {
    SessionState state = SessionState::Closed;
    // The next instant the state changes, and how far away it is.
    std::chrono::sys_seconds nextChange{};
    std::chrono::seconds secondsToChange{ 0 };
    // Verb for that change, chosen from the state it lands in:
    // "closes" / "opens" / "resumes" / "break" — the cell renders
    // "<verb> in <countdown>".
    const char* verb = "opens";
    // The session-local wall clock at the evaluated instant.
    int localHour = 0, localMinute = 0, localSecond = 0;
    bool localIsWeekend = false;
};

inline const char* stateName(SessionState s) {
    switch (s) {
    case SessionState::Open: return "OPEN";
    case SessionState::Break: return "BREAK";
    default: return "CLOSED";
    }
}

namespace detail {
struct Segment {
    std::chrono::sys_seconds begin, end;
    SessionState state;
};
// Local wall time (a trading day + minutes from midnight) -> instant.
// choose::earliest makes the conversion total even if a boundary ever
// landed in a DST gap/overlap (session hours never do — transitions
// happen at 01:00–03:00 local — but a total function can't throw).
inline std::chrono::sys_seconds localToSys(const std::chrono::time_zone* tz,
                                           std::chrono::local_days day, int minutes) {
    auto lt = day + std::chrono::minutes{ minutes };
    return std::chrono::floor<std::chrono::seconds>(tz->to_sys(lt, std::chrono::choose::earliest));
}
inline void addDaySegments(const SessionDef& def, const std::chrono::time_zone* tz,
                           std::chrono::local_days day, Segment* out, int& n) {
    const std::chrono::weekday wd{ day };
    if (wd == std::chrono::Saturday || wd == std::chrono::Sunday) return;
    if (def.breakStartMin >= 0) {
        out[n++] = { localToSys(tz, day, def.openMin), localToSys(tz, day, def.breakStartMin), SessionState::Open };
        out[n++] = { localToSys(tz, day, def.breakStartMin), localToSys(tz, day, def.breakEndMin), SessionState::Break };
        out[n++] = { localToSys(tz, day, def.breakEndMin), localToSys(tz, day, def.closeMin), SessionState::Open };
    } else {
        out[n++] = { localToSys(tz, day, def.openMin), localToSys(tz, day, def.closeMin), SessionState::Open };
    }
}
} // namespace detail

inline SessionStatus evaluate(const SessionDef& def, std::chrono::sys_seconds now) {
    const auto* tz = std::chrono::locate_zone(def.tzName);
    const std::chrono::zoned_time zt{ tz, now };
    const auto localNow = zt.get_local_time();
    const auto localDay = std::chrono::floor<std::chrono::days>(localNow);
    const auto tod = localNow - localDay; // time of day, seconds precision

    SessionStatus st;
    const auto hh = std::chrono::duration_cast<std::chrono::hours>(tod);
    const auto mm = std::chrono::duration_cast<std::chrono::minutes>(tod - hh);
    const auto ss = tod - hh - mm;
    st.localHour = (int)hh.count();
    st.localMinute = (int)mm.count();
    st.localSecond = (int)ss.count();
    const std::chrono::weekday wd{ localDay };
    st.localIsWeekend = (wd == std::chrono::Saturday || wd == std::chrono::Sunday);

    // Segments from yesterday (an instant just after local midnight can
    // still be before today's open — yesterday's segments are never
    // live then, but the scan stays simple and total) through +9 days
    // (a Friday close -> Monday open is the longest regular gap; +9
    // covers long weekends of schedule arithmetic with room to spare).
    detail::Segment segs[40];
    int n = 0;
    for (int d = -1; d <= 9; ++d)
        detail::addDaySegments(def, tz, localDay + std::chrono::days{ d }, segs, n);

    // Inside a segment?
    for (int i = 0; i < n; ++i) {
        if (now >= segs[i].begin && now < segs[i].end) {
            st.state = segs[i].state;
            st.nextChange = segs[i].end;
            // What does the clock land in at nextChange?
            SessionState after = SessionState::Closed;
            for (int j = 0; j < n; ++j)
                if (segs[j].begin == segs[i].end) { after = segs[j].state; break; }
            st.verb = after == SessionState::Break ? "break"
                    : after == SessionState::Open ? "resumes" : "closes";
            st.secondsToChange = std::chrono::duration_cast<std::chrono::seconds>(st.nextChange - now);
            return st;
        }
    }
    // Closed: the next segment begin is the next change.
    st.state = SessionState::Closed;
    for (int i = 0; i < n; ++i) {
        if (segs[i].begin > now) {
            st.nextChange = segs[i].begin;
            st.verb = "opens";
            st.secondsToChange = std::chrono::duration_cast<std::chrono::seconds>(st.nextChange - now);
            return st;
        }
    }
    return st; // unreachable with the +9 day horizon; Closed with no countdown
}

// "2h 14m" / "7h 03m" / "47h 30m" — hours unpadded (they can exceed a
// day across a weekend), minutes always two digits.
inline std::string formatCountdown(std::chrono::seconds s) {
    long long total = s.count();
    if (total < 0) total = 0;
    const long long h = total / 3600, m = (total % 3600) / 60;
    std::string out = std::to_string(h) + "h ";
    if (m < 10) out += "0";
    out += std::to_string(m) + "m";
    return out;
}
// The full cell line: "closes in 2h 14m".
inline std::string countdownText(const SessionStatus& st) {
    return std::string(st.verb) + " in " + formatCountdown(st.secondsToChange);
}
// Session-local clock for the cell: "09:41" (24h) or "9:41 AM" (12h).
inline std::string formatLocalTime(const SessionStatus& st, bool use24h) {
    if (use24h) {
        std::string out;
        if (st.localHour < 10) out += "0";
        out += std::to_string(st.localHour) + ":";
        if (st.localMinute < 10) out += "0";
        out += std::to_string(st.localMinute);
        return out;
    }
    int h = st.localHour % 12;
    if (h == 0) h = 12;
    std::string out = std::to_string(h) + ":";
    if (st.localMinute < 10) out += "0";
    out += std::to_string(st.localMinute);
    out += st.localHour < 12 ? " AM" : " PM";
    return out;
}
} // namespace sessionbar
