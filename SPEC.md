# SessionBar — SPEC (v0.1)

Third sibling in Jackson's "bar" family: MeterBar (audio meters),
TickerBar (crypto prices), SessionBar (market session clocks).
Approved by Jackson 2026-10-28: "market session clocks (New York /
London / Asia opens and closes with countdowns). Every trader I've
seen keeps one of these somewhere."

## What it is

A borderless, always-on-top Windows desktop bar showing the state of
the three big equity sessions, each in its own cell:

| Cell     | Exchange | Regular hours (local)     | Zone             |
|----------|----------|---------------------------|------------------|
| NEW YORK | NYSE     | 09:30–16:00               | America/New_York |
| LONDON   | LSE      | 08:00–16:30               | Europe/London    |
| TOKYO    | TSE      | 09:00–15:00, lunch 11:30–12:30 | Asia/Tokyo  |

Each cell shows: city name, the session's current local time, the
state (OPEN green / BREAK amber / CLOSED red), and a live countdown to
the next state change — "closes in 2h 14m" while open, "opens in
7h 03m" while closed, "break in 0h 30m" in Tokyo's morning, "resumes
in 0h 22m" during Tokyo's lunch break. Weekends count down to Monday's
open. Countdowns re-evaluate every second.

## Rules that matter

- **DST is computed, never assumed.** Session instants are built as
  local wall-clock times in each exchange's IANA zone and converted
  with std::chrono's tz database (C++20). No fixed UTC offsets
  anywhere. The US/EU mismatch weeks (March, late Oct/early Nov) are
  pinned by unit tests against the published 2026 DST calendars.
- **Holidays are NOT modeled in v0.1.** The clock follows the regular
  Mon–Fri schedule; on an exchange holiday a cell will wrongly say
  OPEN. The README and the bar's tooltip both say this plainly.
- **Pure core.** All session/countdown math lives in
  `core/include/sessionbar/Sessions.hpp` as pure functions of an
  injected instant, unit-tested (107 checks incl. boundary minutes at
  open/close/break edges, weekend rollover, both DST-mismatch weeks).
- **Family behaviour.** Drag to move, position remembered; dock top or
  bottom; Settings: session toggles, 12h/24h cell clocks, Midnight
  Kids theme (default, MeterBar palette verbatim) + Clean Dark,
  background style + opacity; Reserve screen space (Windows AppBar,
  ported from MeterBar v0.18.1 via the same route TickerBar took:
  event-driven ReserveMachine with dock hysteresis, top/bottom edges,
  toggle default OFF); one-shot Arrange/Restore sweep.
- **No network, no accounts, no data leaves the machine.** The only
  inputs are the system clock and the OS time-zone database.

## Out of scope for v0.1

Exchange holiday calendars; futures/overnight sessions; more
exchanges (HKEX, Euronext, ASX); alerts at open/close; per-cell
click actions.
