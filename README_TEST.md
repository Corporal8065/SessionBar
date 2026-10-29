# SessionBar — v0.1 test kit

A borderless always-on-top **market session clock bar** for Windows — the
third sibling in the bar family (MeterBar = audio meters, TickerBar =
crypto prices). One cell per session:

- **NEW YORK** — NYSE, 9:30 AM–4:00 PM New York time
- **LONDON** — LSE, 8:00 AM–4:30 PM London time
- **TOKYO** — TSE, 9:00 AM–3:00 PM Tokyo time, with the 11:30 AM–12:30 PM
  lunch break shown as a BREAK state

Each cell shows the city, **that city's current local time**, the state
(**OPEN** green / **BREAK** amber / **CLOSED** red), and a **live
countdown** to the next change: "closes in 2h 14m" while open, "opens in
7h 03m" while closed, "break in 0h 30m" in Tokyo's morning session,
"resumes in 0h 22m" during Tokyo's lunch. Weekends count down to Monday's
open. Countdowns tick every second.

**Daylight saving is computed from the real IANA time-zone rules**, never
from fixed offsets — the weeks in March and late October when the US and
Europe change clocks on different Sundays are handled correctly (the
core math is unit-tested against the published 2026 calendars).

**Honesty note — holidays are NOT modeled in v0.1.** The clock follows
the regular Monday–Friday schedule only. On an exchange holiday (July 4,
Christmas, a UK bank holiday, a Japanese national holiday) the cell will
still say OPEN during regular hours. A holiday calendar is future work;
v0.1 would rather tell you this plainly than pretend.

No network, no accounts, nothing leaves the machine — the only inputs
are your system clock and the Windows time-zone database. Needs Windows
10 or 11 (the time-zone rules ship with the OS).

No exe exists yet — the one-click build below makes it (the proven
MeterBar/TickerBar flow).

## 1. First: build it (about 5 minutes, one time)

You need Visual Studio 2022 or 2026 with the **Desktop development with
C++** workload (the same setup as MeterBar's test builds). Then:

1. Unzip this kit somewhere normal (Desktop is fine).
2. Open the **sessionbar-app** folder.
3. Double-click **build.bat**. Keep it open — it finds CMake by itself
   (PATH, standalone installs, vswhere, manual scan), configures (the
   first run downloads JUCE 8 — needs internet), builds Release, and
   runs the core unit tests at the end. You should see
   **107 checks, 0 failures**.
4. When it says DONE, double-click
   `build-app\app\SessionBar_artefacts\Release\SessionBar.exe`.

**SmartScreen / unsigned-app note:** the exe is unsigned free software,
so Windows may show a blue "Windows protected your PC" prompt. Click
**More info → Run anyway**. Same for the optional installer later.

Everything the build checked is in `build-log.txt`. If anything fails,
send Milo that file.

## 2. Field tests — what to click and what you should see

1. **First launch.** A short "Made with JUCE" splash (~1.5 s), then the
   bar appears **borderless and always-on-top**, Midnight Kids dark with
   pink accents: three cells — NEW YORK, LONDON, TOKYO — each with a
   local time, a state, and a countdown already ticking down once a
   second. If it opens somewhere odd, just drag it.
2. **Glance check against your phone.** Open a world clock on your phone
   (or timeanddate.com) and compare: each cell's time should match that
   city's time **to the minute**, and each state should match whether
   that market is actually open right now. This is the whole product in
   one glance — if a cell disagrees with the phone, that's the bug to
   report (tell Milo the time you saw it).
3. **Countdown near an open.** If you can, watch a cell in the last
   couple of minutes before an open or close (New York opens 9:30 AM
   Eastern — 6:30 AM Pacific in winter, e.g.). The countdown should hit
   "0h 00m" and the state should flip at the same moment your phone's
   clock rolls to the session time — not a minute early or late.
4. **Tokyo lunch.** Between 11:30 AM and 12:30 PM Tokyo time (7:30–8:30
   PM Pacific the previous evening), the TOKYO cell should say **BREAK**
   in amber with "resumes in …". Before 11:30 it says "break in …"
   during the morning session.
5. **Weekend.** On a Saturday or Sunday, all three cells say CLOSED and
   count down to **Monday's** open (New York's countdown will read
   40+ hours on a Saturday — that's correct).
6. **Drag + it remembers.** Drag the bar anywhere by an empty part of
   its background (or by a cell). Close it (the **X** at the right end),
   relaunch: it comes back in the same place, same size.
7. **Settings — sessions + clock.** Gear button → Settings. Untick
   London: its cell disappears. Tick it back. Flip **24-hour clock**:
   cell times switch between "9:41 AM" and "09:41".
   Restart the app: your choices persisted.
8. **Appearance.** Settings → Appearance: switch Theme between
   **Midnight Kids** and **Clean Dark**, Background style between
   Midnight Kids / Clean Dark / Flat, and drag Background opacity down —
   the desktop shows through the bar's background (cells stay readable).
9. **Dock + Reserve screen space.** Settings → Window → **Dock bottom**:
   the bar snaps to the bottom edge. Turn on **Reserve** (toolbar button
   or the Settings toggle), then maximise a browser window: on a top
   dock it starts **below** the bar; on a bottom dock it ends **above**
   the bar — Windows shrinks the work area around the bar. Drag the bar
   away from the edge: the reservation releases (maximised windows can
   use the full screen again) and re-arms when you dock again. Turn
   Reserve off: everything returns to normal. Reserve is **off by
   default** and strictly opt-in.
10. **Arrange / Restore.** With the bar docked, Settings → Window →
    **Arrange windows**: other open windows on that monitor slide clear
    of the bar. **Restore windows** puts them back where they were.

## 3. Sharing it (optional)

Double-click **build-release.bat**: it builds and stages
`dist\SessionBar.exe`. If Inno Setup 6 is installed (free:
https://jrsoftware.org/isdl.php) it also makes
`dist\SessionBar-Setup.exe` — the single file to send a friend. They
double-click it; no compiler or Visual Studio needed on their side
(Windows will show the SmartScreen prompt from §1 — More info → Run
anyway).
