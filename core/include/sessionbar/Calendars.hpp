#pragma once
#include <chrono>
#include <optional>
#include <string>
namespace sessionbar {
// EXCHANGE CALENDARS — v0.2.0 (item g): full-day holidays and early
// closes for NYSE / LSE / JPX (Tokyo), embedded as data in the pure
// core — fully offline, no network, no OS calls. Sessions.hpp consults
// this table when building a day's segments: a holiday produces NO
// segments (not even New York pre/post); an early close replaces the
// day's closeMin (New York post-market still runs to 20:00 after a
// 13:00 early close — extended hours continue in reality).
//
// SOURCES + HONESTY (researched 2026-10-29):
//  - NYSE 2026–2028: ICE/NYSE published holiday + early-close lists.
//    Note: there is NO New Year's Day closure in calendar 2028
//    (Jan 1 2028 is a Saturday and is not observed), and no early
//    close on 2026-07-02, 2027-07-02, 2027-12-24 or 2028-12-24 —
//    their absence here is deliberate, matching the published lists.
//  - LSE 2026–2028: England & Wales bank holidays (LSE closes on
//    them); early close = market closes 12:30 London on Christmas
//    Eve / New Year's Eve when they are trading days. The two 2028
//    early closes (Dec 22, Dec 29) follow the 2023 analogue — both
//    eves fall on Sunday in 2028, and in 2023 the Fridays before them
//    were half days — and are marked DERIVED below: strongly inferred,
//    not yet in an official LSE 2028 notice at research time.
//  - JPX 2026–2027: the official JPX "Market Holidays" page (cash
//    market). JPX 2028 was NOT yet published at research time; the
//    2028 rows are DERIVED from Japan's statutory national holidays
//    plus JPX's standing Jan 1–3 / Dec 31 market holidays, and are
//    marked DERIVED. Tokyo has no early closes in these tables.
// Every date below is pinned by a unit test (tests/test_core.cpp).

struct CalDate { int y, m, d; };
struct EarlyClose { int y, m, d, closeMin; };

namespace cal {
// ---- NYSE (America/New_York) ----
inline constexpr CalDate kNyseHolidays[] = {
    // 2026
    {2026,1,1},{2026,1,19},{2026,2,16},{2026,4,3},{2026,5,25},{2026,6,19},
    {2026,7,3},{2026,9,7},{2026,11,26},{2026,12,25},
    // 2027
    {2027,1,1},{2027,1,18},{2027,2,15},{2027,3,26},{2027,5,31},{2027,6,18},
    {2027,7,5},{2027,9,6},{2027,11,25},{2027,12,24},
    // 2028 (no New Year closure — see header note)
    {2028,1,17},{2028,2,21},{2028,4,14},{2028,5,29},{2028,6,19},
    {2028,7,4},{2028,9,4},{2028,11,23},{2028,12,25},
};
inline constexpr EarlyClose kNyseEarlyCloses[] = { // 13:00 ET
    {2026,11,27,13*60},{2026,12,24,13*60},
    {2027,11,26,13*60},
    {2028,7,3,13*60},{2028,11,24,13*60},
};
// ---- LSE (Europe/London) ----
inline constexpr CalDate kLseHolidays[] = {
    // 2026
    {2026,1,1},{2026,4,3},{2026,4,6},{2026,5,4},{2026,5,25},{2026,8,31},{2026,12,25},{2026,12,28},
    // 2027
    {2027,1,1},{2027,3,26},{2027,3,29},{2027,5,3},{2027,5,31},{2027,8,30},{2027,12,27},{2027,12,28},
    // 2028
    {2028,1,3},{2028,4,14},{2028,4,17},{2028,5,1},{2028,5,29},{2028,8,28},{2028,12,25},{2028,12,26},
};
inline constexpr EarlyClose kLseEarlyCloses[] = { // 12:30 London
    {2026,12,24,12*60+30},{2026,12,31,12*60+30},
    {2027,12,24,12*60+30},{2027,12,31,12*60+30},
    {2028,12,22,12*60+30},{2028,12,29,12*60+30}, // DERIVED (2023 analogue) — see header
};
// ---- JPX / Tokyo (Asia/Tokyo) ----
inline constexpr CalDate kJpxHolidays[] = {
    // 2026 (official JPX)
    {2026,1,1},{2026,1,2},{2026,1,3},{2026,1,12},{2026,2,11},{2026,2,23},{2026,3,20},
    {2026,4,29},{2026,5,4},{2026,5,5},{2026,5,6},{2026,7,20},{2026,8,11},{2026,9,21},
    {2026,9,22},{2026,9,23},{2026,10,12},{2026,11,3},{2026,11,23},{2026,12,31},
    // 2027 (official JPX)
    {2027,1,1},{2027,1,2},{2027,1,3},{2027,1,11},{2027,2,11},{2027,2,23},{2027,3,22},
    {2027,4,29},{2027,5,3},{2027,5,4},{2027,5,5},{2027,7,19},{2027,8,11},{2027,9,20},
    {2027,9,23},{2027,10,11},{2027,11,3},{2027,11,23},{2027,12,31},
    // 2028 DERIVED (statutory holidays + JPX standing Jan 1–3 / Dec 31) — see header
    {2028,1,3},{2028,1,10},{2028,2,11},{2028,2,23},{2028,3,20},{2028,5,3},{2028,5,4},
    {2028,5,5},{2028,7,17},{2028,8,11},{2028,9,18},{2028,9,22},{2028,10,9},{2028,11,3},
    {2028,11,23},{2028,12,31},
};
} // namespace cal

namespace detail {
inline bool dateIn(const CalDate* table, size_t n, std::chrono::year_month_day ymd) {
    const int y = (int)ymd.year(), m = (int)(unsigned)ymd.month(), d = (int)(unsigned)ymd.day();
    for (size_t i = 0; i < n; ++i)
        if (table[i].y == y && table[i].m == m && table[i].d == d) return true;
    return false;
}
inline std::optional<int> earlyIn(const EarlyClose* table, size_t n, std::chrono::year_month_day ymd) {
    const int y = (int)ymd.year(), m = (int)(unsigned)ymd.month(), d = (int)(unsigned)ymd.day();
    for (size_t i = 0; i < n; ++i)
        if (table[i].y == y && table[i].m == m && table[i].d == d) return table[i].closeMin;
    return std::nullopt;
}
} // namespace detail

// defId is the SessionDef id ("NY" / "LDN" / "TYO"). Unknown ids have
// no calendar: no holidays, no early closes (v0.1 behaviour).
inline bool isExchangeHoliday(const char* defId, std::chrono::year_month_day ymd) {
    const std::string id = defId ? defId : "";
    if (id == "NY")  return detail::dateIn(cal::kNyseHolidays, sizeof(cal::kNyseHolidays) / sizeof(CalDate), ymd);
    if (id == "LDN") return detail::dateIn(cal::kLseHolidays,  sizeof(cal::kLseHolidays)  / sizeof(CalDate), ymd);
    if (id == "TYO") return detail::dateIn(cal::kJpxHolidays,  sizeof(cal::kJpxHolidays)  / sizeof(CalDate), ymd);
    return false;
}
inline std::optional<int> earlyCloseMin(const char* defId, std::chrono::year_month_day ymd) {
    const std::string id = defId ? defId : "";
    if (id == "NY")  return detail::earlyIn(cal::kNyseEarlyCloses, sizeof(cal::kNyseEarlyCloses) / sizeof(EarlyClose), ymd);
    if (id == "LDN") return detail::earlyIn(cal::kLseEarlyCloses,  sizeof(cal::kLseEarlyCloses)  / sizeof(EarlyClose), ymd);
    return std::nullopt; // TYO: no early closes
}
} // namespace sessionbar
