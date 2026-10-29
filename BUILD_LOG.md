# SessionBar — BUILD_LOG

## v0.2.0 — 2026-10-29 (exchange calendars + New York PRE/POST + family release flow)

- **(g) Exchange calendars.** New pure-core header
  `Calendars.hpp`: NYSE / LSE / JPX holidays and early closes for
  2026–2028 as embedded data (fully offline — SessionBar still makes
  no network calls). 29 NYSE holidays + 5 early closes (13:00 ET);
  24 LSE holidays + 6 early closes (12:30 London); 55 JPX holidays,
  no early closes. `addDaySegments` now skips a holiday entirely and
  substitutes an early close for the day's closeMin.
  **Provenance, honestly:** 2026–2027 (and NYSE/LSE 2028 holidays)
  from the published exchange lists researched 2026-10-29; **Tokyo
  2028 is DERIVED** from Japan's statutory national holidays plus
  JPX's standing Jan 1–3 / Dec 31 market holidays (JPX had not
  published 2028), and **the LSE's two late-December 2028 early
  closes (Dec 22, Dec 29) are DERIVED** from the 2023 analogue
  (both eves fall on Sunday in 2028; in 2023 the preceding Fridays
  were half days). Deliberate absences match the published lists:
  NYSE has NO New Year closure in calendar 2028 and no early close
  on 2026-07-02 / 2027-12-24. Every date is pinned by a unit test;
  the absence cases are tested too.
- **(h) New York PRE / POST.** `SessionState` gains Pre and Post;
  NY's day is now PRE 04:00–09:30 → OPEN 09:30–16:00 → POST
  16:00–20:00 (all ET). Verbs: PRE "opens in", Open→Post "closes",
  POST "ends in", overnight "pre-market opens in". Cells render PRE
  amber and POST teal (family palette). On an early-close day POST
  starts at the early close and still runs to 20:00; on a holiday
  there is no PRE/POST at all. Superseded v0.1 expectations were
  updated deliberately — including one the calendar changed for
  real: Saturday Jan 17 2026's next change is now Tuesday's PRE,
  because Monday Jan 19 is MLK Day.
- **Release workflow (Jackson's addition):**
  `.github/workflows/build-windows.yml`, adapted from MeterBar's —
  a `v*` tag builds the app, runs the core suite, compiles the Inno
  installer and attaches `SessionBar-Setup.exe` to the tag's GitHub
  Release; the job refuses a tag that doesn't match the CMakeLists
  version. `SessionBar.iss` gained the `#ifndef MyAppVersion` guard.
  **No tag was created by this work.**
- **README + theme parity (Jackson's additions).** README.md rebuilt
  in MeterBar's structure (centred header, CI badge, Features /
  Install / Build / Gallery / family). Gallery = screenshots-wanted
  list ONLY (standing honesty rule: no borrowed or faked images; no
  Windows captures of SessionBar exist yet). Theme parity verified:
  the default Midnight Kids palette is value-for-value identical to
  MeterBar's default theme.
- The app tooltip and Settings note that said "holidays are not
  modeled" now say they are — v0.1's one honest limit is retired.

Kit file: `sessionbar-test-kit-v0.2.0.zip` — same layout as v0.1,
plus `REQUESTS.md`, `.github/workflows/build-windows.yml` and
`core/include/sessionbar/Calendars.hpp`.

Verified (Linux VM; Windows behaviour is the README field tests' job):
- **Core suite: 255 checks, 0 failures** (was 107) — in tree AND
  rebuilt from the shipped kit contents. New: 119 per-date calendar
  checks, deliberate-absence checks, calendar×PRE/POST integration
  (Thanksgiving, early closes, Easter Monday, Coming-of-Age Day,
  PRE/POST boundary minutes), and the updated PRE/POST expectations.
- **App translation units syntax-checked clean** against JUCE 8.0.4
  headers. The full CMake app build could not run on this VM (the
  nested juceaide build needs libxrandr-dev, not installed — same
  environment gap as TickerBar v0.2.0, recorded in REQUESTS.md);
  the CI workflow builds the app on windows-latest.

Honest limits: the calendar tables end at 2028 — a 2029 refresh is a
data update, not a code change. Tokyo 2028 and the two LSE 2028
early closes are derived, as flagged above and in the code.

## v0.1 — 2026-10-28 (first build)

New sibling in the bar family (MeterBar = audio meters, TickerBar =
crypto prices): market session clocks for NEW YORK / LONDON / TOKYO
with live countdowns. Spec: SPEC.md (Jackson, approved 2026-10-28).

### What was built

- **Pure core** (`core/include/sessionbar/Sessions.hpp`): session
  evaluation as a pure function of an injected instant. Session
  open/close/break instants are constructed as local wall-clock times
  in each exchange's IANA zone via std::chrono `zoned_time` and
  converted with the real tz database — no fixed UTC offsets anywhere,
  so DST (including the US/EU mismatch weeks) is computed, not assumed.
  Sessions: NYSE 09:30–16:00 America/New_York; LSE 08:00–16:30
  Europe/London; TSE 09:00–15:00 Asia/Tokyo with the 11:30–12:30 lunch
  as a first-class BREAK segment. Countdown verbs: closes / opens /
  resumes / break. Also in the core: `Arrange.hpp` (window-fit math,
  copied from TickerBar), `ReserveState.hpp` (MeterBar v0.18.1's
  event-driven reserve machine, generalized with the docked-edge
  parameter), `State.hpp` (SessionBarState defaults).
- **Core tests: 107 checks, 0 failures** — mid-session, pre-open,
  post-close, Tokyo lunch both sides and its exact boundary minutes,
  at-open/at-close boundary minutes for all three sessions, weekend
  rollover to Monday for all three, Friday-close→Monday, the March
  2026 US-before-EU DST week (incl. Tue Mar 10, when New York opens
  under EDT while London is still GMT), the late-Oct 2026 EU-before-US
  week (Tue Oct 27: London GMT, New York EDT), a summer sanity day,
  countdown/clock formatting (12h/24h), arrange math, the reserve
  machine's transition table, state defaults. One expectation was
  corrected during the build: 24h midnight formats as "00:00".
- **App** (C++20/JUCE 8, family layout): borderless always-on-top bar,
  one cell per visible session (city, session-local time, OPEN green /
  BREAK amber / CLOSED red, countdown), 1 Hz refresh, drag anywhere,
  position persisted. Settings (family rebuilt-on-open pattern):
  session toggles, 12h/24h, theme (Midnight Kids default — palette
  values verbatim from MeterBar/TickerBar — plus Clean Dark),
  background style + opacity, pin toggle, dock top/bottom, and the
  settings file lives at %APPDATA%\SessionBar\settings.json.
- **Reserve screen space**: MeterBar v0.18.1's ScreenSpace AppBar
  service ported the same way TickerBar ported it (requested-rect
  idempotence, setAppBarPosIfChanged, write-once arrange snapshots,
  GUI_INMOVESIZE re-entrancy guard, event-driven ReserveMachine fed
  from real events — toggle, settings load, dock change, window
  moved/resized), generalized so the machine tracks the DOCKED edge:
  reserve + Arrange/Restore work on top and bottom (Jackson's
  MiniMeters semantics: reserve follows where the bar lives). Toggle
  default OFF. The whole service compiles only under the compiler's
  own _WIN32 macro (SESSIONBAR_SCREENSPACE_WINDOWS).

### Verification (this machine, Linux VM — Jackson's Windows run is the field test)

- Core tests 107/107 in the tree (both the core-only build and the
  test_core target inside the full app build).
- Full app build in the tree: exit 0 (SessionBar binary, 8.4 MB).
  Xvfb smoke: app alive after 14 s, settings.json written to the
  app-data folder. (One configure attempt died to a transient
  missing-header error under heavy multi-build load on this 2-core
  VM; the rerun was clean.)
- MinGW (Windows-target) checks: core headers clean under
  -std=c++20; ScreenSpace.cpp full syntax+type check clean with NO
  forced _WIN32 (the guard keys off the compiler's own macro);
  A/B preprocess of ScreenSpace.cpp: the Windows-only "appbar
  active" string appears 1× under the Windows target, 0× under
  Linux. Main.cpp and MainComponent.cpp also pass the MinGW
  syntax+type check (they need <cstring>/<cstdlib> pre-included —
  JUCE's own headers assume MSVC's leakier header ordering under
  MinGW; the real MSVC toolchain is unaffected).
- MSVC trap sweep over app sources: no bare min/max macro-risk calls,
  no POSIX-only headers, windows.h only behind JUCE_WINDOWS/_WIN32
  with NOMINMAX defined first.
- Packaging follows the family discipline: build.bat /
  build-release.bat (ASCII + CRLF, goto-label CMake discovery,
  build-release falls back to dist\SessionBar.exe + the Inno Setup
  note when ISCC is absent), SessionBar.iss (fresh AppId GUID).

### Honest limitations

- **Exchange holidays are NOT modeled in v0.1** — the clock follows
  the regular Mon–Fri schedule; on a holiday a cell will say OPEN.
  Stated in README_TEST.md, SPEC.md, the bar tooltip, and Settings.
- AppBar reserve/arrange behaviour is code- and preprocess-proven
  here, not runtime-proven — this VM is Linux; the Windows field test
  is Jackson's run (same status as MeterBar/TickerBar v0.1).
- Icons are the family placeholder icons copied from TickerBar.

## Addendum (same day) — (k) family linking landed

Core `sessionbar/BarLink.hpp` + app `BarLink.h` + on-bar Link toggle,
as detailed in REQUESTS.md (k). Suite after: **268 checks, 0
failures** (255 + 13 protocol-geometry checks). The multi-process
behaviour needs a live two-bar field test on Windows.
