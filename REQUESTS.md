# SessionBar — REQUESTS (per-item status ledger)

Verification rule (Jackson, 2026-10-29): every item is DONE only with
proof — the full core suite green after the change, new checks for new
behaviour, and errors recorded here with their actual text, never hidden.

## v0.2.0 (2026-10-29)

- (g) **Exchange holiday + early-close calendars — DONE.** NYSE, LSE
  and JPX (Tokyo) calendars for 2026–2028 embedded as data in the pure
  core (`core/include/sessionbar/Calendars.hpp`), fully offline:
  29 NYSE holidays + 5 early closes (13:00 ET), 24 LSE holidays + 6
  early closes (12:30 London), 55 JPX holidays (no early closes). A
  holiday produces no segments at all (not even NY pre/post); an
  early close ends the regular session early while NY post-market
  still runs to 20:00. **Every date is a unit test** (119 per-date
  checks), plus integration checks (Thanksgiving closed incl. no PRE,
  day-after-Thanksgiving 13:00 close into POST, LSE Christmas-Eve
  12:30 close, Easter Monday closed, Tokyo Coming-of-Age Day closed).
  Honest provenance, also in the header and BUILD_LOG: NYSE/LSE/JPX
  2026–2027 from the published lists; **Tokyo 2028 is DERIVED** from
  Japan's statutory holidays (JPX had not published 2028 at research
  time) and **the two LSE late-December 2028 early closes are
  DERIVED** from the 2023 analogue. Deliberate absences are tested
  too: no NYSE New Year closure in 2028, no 2026-07-02 early close.
- (h) **New York PRE / POST states — DONE.** PRE 04:00–09:30 ET
  (amber) and POST 16:00–20:00 ET (teal) are first-class session
  states in the core (`SessionState::Pre/Post`), with verbs: PRE
  counts down "opens in", Open→Post reads "closes", POST counts down
  "ends in", and the overnight CLOSED counts down to "pre-market
  opens". Previously pinned expectations that PRE/POST supersede
  were updated deliberately (08:00 EST is PRE, 17:00 EST is POST,
  exactly-16:00 is POST) — and the Saturday test now lands on
  Tuesday because Monday Jan 19 2026 is MLK Day, an interaction the
  new calendar produced and the test now pins.
- **Release workflow — DONE.** `.github/workflows/build-windows.yml`
  (adapted from MeterBar's): on a `v*` tag it builds the app, runs
  the core suite, compiles the Inno installer and attaches
  `SessionBar-Setup.exe` to that tag's GitHub Release; it refuses to
  release when the tag doesn't match the CMakeLists version. No tag
  has been created by this work — tagging stays with Jackson/Milo.
  `SessionBar.iss` gained MeterBar's `#ifndef MyAppVersion` guard so
  the CI `/DMyAppVersion` override works.
- **README parity + theme parity — DONE.** README.md restructured to
  MeterBar's format (centred header, build badge, Features / Install /
  Build from source / Gallery / The family). Gallery ships as a
  screenshots-wanted list only — no Windows captures of SessionBar
  exist yet, and no other product's images are used (standing gallery
  honesty rule). Theme: SessionBar's default is the family's
  Midnight Kids palette — verified value-for-value identical to
  MeterBar's default theme (it already was; the old README said
  "verbatim" and the values back it).
- (j) **UI parity — PARTIAL.** SessionBar already carried the family
  conventions (event-driven reserve machine, on-bar Reserve/Dock
  controls, MeterBar settings idiom). This release aligns the
  remaining content (PRE/POST colours in the family palette).
- (k) **Inter-app linking — OPEN.** Not landed in v0.2.0; to be built
  across all three apps together (see TickerBar's REQUESTS.md).

## Errors recorded during this work

- One test initially failed for the right reason: the Saturday
  weekend test expected Monday Jan 19 2026 segments, but that Monday
  is MLK Day — the new calendar is correct and the expectation was
  updated to Tuesday's PRE (237600 s) with the reason recorded in
  the test itself.
- The full JUCE app build shares this VM's environment gap
  (no libxrandr-dev for the nested juceaide build); app translation
  units were verified with `g++ -fsyntax-only` against JUCE 8.0.4
  headers — clean. The Windows CI workflow compiles the real thing.
