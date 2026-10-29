# SessionBar — REQUESTS (per-item status ledger)

Verification rule (Jackson, 2026-10-29): every item is DONE only with
proof — the full core suite green after the change, new checks for new
behaviour, and errors recorded here with their actual text, never hidden.

## v0.2.1 (2026-10-29, field-test fix round)

Jackson field-tested v0.2.0 on Windows the same night and filed four
critiques. This round answers all four; SessionBar is the only app
with a version bump — the shared-core corrections also landed on
MeterBar's and TickerBar's mains (no bump/tag there tonight; they
release tomorrow).

- (j) **UI parity — DONE (the named remainder is closed).** The
  control cluster now sits at the bar's top right exactly as
  MeterBar's toolbar: **Reserve | Link | Arrange | Restore | Dock |
  Settings | _ | Quit**, same order, same widths as MeterBar's
  buttons, same height. New on the bar: **Arrange** and **Restore**
  (were only in Settings; Settings keeps its copies) and **Dock** —
  the v0.2.0 remainder. The Dock button's label mirrors the current
  edge ("Dock Top" / "Dock Bot"); a click flips the edge and snaps;
  the Settings dock buttons refresh the label through the same path.
- **Family chrome — DONE.** The v0.2.0 buttons wore stock JUCE
  colours; they now wear MeterBar's toolbar colouring verbatim
  (panel face, deepest "on" face, accent text off, body text on —
  `applyChrome()`, re-applied on theme change), and the version
  label matches MeterBar's 11 pt (was 10 pt). Supporting changes:
  minimum window width 560 → 700 and the fresh-install default width
  880 → 1060 so the full cluster leaves the session cells readable
  (saved window sizes are untouched).
- **Arrange alignment (field fix 2) — DONE.** Root cause: the link
  protocol's group-dock geometry (`groupDockRect` in the pure core)
  moved a stacked follower along the dock axis only — SessionBar
  stacked under MeterBar vertically but kept its own x, so it did
  not sit under a sideways-moved MeterBar. Fix: stacking now also
  aligns the cross axis — **left edges aligned** for top/bottom
  stacks, top edges for left/right stacks. Identical correction in
  all three cores (MeterBar/TickerBar copies landed on main, no
  version bump tonight). Pinned by new core checks in all three
  suites (SessionBar 268 → 292, TickerBar 320 → 343, MeterBar
  926 → 949, all 0 failures).
- **Background linking (field fix 3) — DONE, with one honest limit
  below.** The end-to-end path (Settings change → publish → shared
  memory → follower apply) was traced in all three apps. Defects
  proven from the v0.2.0 code and fixed:
  1. **Echo:** a follower that adopted a peer's background
     re-published the adopted values as its own change and bumped
     its bgSeq, so the originator adopted its own values back.
  2. **Seq inflation:** the settings feed force-published on EVERY
     settings interaction, bumping bgSeq whether or not the
     background had changed.
  3. **Adoption starvation:** adoption was evaluated only after the
     drag-follow section, whose early return let a live peer drag
     postpone it indefinitely.
  4. **Missing feed:** MeterBar never called
     `notifyLocalSettings()` at all — its publishes rode on the
     client poll alone (fixed on MeterBar's main).
  The publish/adopt decisions now live in the pure core
  (`BgSyncState`, `bgIsLocalChange`, `bgAdoption` in BarLink.hpp)
  and are unit-tested in all three suites, including the echo rule
  (adopted values ARE the adopter's published values) and the
  last-in-slot-order rule for simultaneous peer changes; the three
  app clients were rewired onto them (verified identical modulo
  namespace). SessionBar's applyBg also rebuilds an open Settings
  window so it never displays pre-adoption values.
  **Honest limit:** the two-process transport itself (named shared
  memory + 10 ms mutex + JUCE window plumbing) cannot be exercised
  on this VM — what is proven here is the protocol logic (core
  suites) and the app wiring (syntax-checked translation units).
  The live Windows two-bar test remains the end-to-end proof, and
  it requires Link ON in both bars (the toggle defaults OFF).

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
- (j) **UI parity — PARTIAL (one named remainder).** SessionBar already carried the family
  conventions (event-driven reserve machine, on-bar Reserve/Dock
  controls, MeterBar settings idiom). This release aligns the
  remaining content (PRE/POST colours in the family palette).
  **The remainder (Dock on the bar) closed in v0.2.1 — see above.**
- (k) **Inter-app linking — DONE (build).** Landed across all three
  apps in the P4 commits, which ARE included in the v0.2.0 tag
  (SessionBar v0.2.0 → 7703545). This line originally read OPEN,
  written before the P4 commits landed; corrected in the v0.2.1
  round. v0.2.1 then fixed the group-dock cross-axis alignment and
  hardened the background sync — see above. A live two-bar Windows
  field test is still owed.

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
- v0.2.1 round: the MeterBar syntax check initially reported 10
  BinaryData member errors — the checker's include path pointed at
  a JUCE demo's generated BinaryData.h (AudioPluginHost's), not
  MeterBar's own. With the app's own generated header the unit is
  clean (0 errors). A checker-harness mistake, recorded rather than
  hidden; no app code was involved.
