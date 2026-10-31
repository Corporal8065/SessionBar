# Code signing policy — SessionBar

*DRAFT — to be published in the SessionBar repository (e.g. as
`CODE_SIGNING_POLICY.md`) and linked from the README's "Code signing policy"
section before the SignPath subscription is used. Status until SignPath
approves the project: **code signing is not active yet** — current releases
are unsigned, so Windows SmartScreen may warn about an unknown publisher.*

## What is signed

Only the Windows release files of SessionBar:

- the release installer, `SessionBar-Setup.exe`, and
- the application executable inside it, `SessionBar.exe`.

Both are built **only** by the GitHub Actions workflow in this repository
(`.github/workflows/build-windows.yml`, Windows runner, from a version tag),
from the source code in this public repository. Nothing built on a personal
machine is signed or released. Development builds are unsigned.

Third-party components inside the installer keep their own signatures or
remain unsigned, as applicable: the installer is produced with Inno Setup,
and SessionBar is built with the JUCE framework (see "Upstream components"
below).

## Who signs

Free code signing provided by SignPath.io, certificate by SignPath Foundation.

Releases are signed under a certificate issued to **SignPath Foundation**, so
Windows will show **"SignPath Foundation"** as the publisher of signed
SessionBar files. That is expected and correct. The signing key is held by
SignPath Foundation on its HSM and is never present on any project machine.

## Build and origin verification

Every signing request is submitted by this repository's GitHub Actions
workflow for a build of this repository. SignPath verifies that the signed
binary originates from this official repository and the exact commit that was
built. Every release's signing request is **approved manually** by the
project's approver (below) before SignPath signs it.

## Team roles

SessionBar is a one-person project.

- **Committers and reviewers:** Jackson (GitHub
  [Corporal8065](https://github.com/Corporal8065)) — sole committer; also
  reviews any external pull requests himself before merge.
- **Approvers:** Jackson (GitHub
  [Corporal8065](https://github.com/Corporal8065)) — the only person who can
  approve a signing request.

All team members (just Jackson) use multi-factor authentication on both
GitHub and SignPath.

## Upstream components

- **JUCE** (https://juce.com) — the application framework, fetched by CMake
  from the official JUCE GitHub release (8.0.4) during the public CI build.
  JUCE modules are dual-licensed (AGPLv3 / commercial JUCE licence); this
  project uses JUCE under its open-source (AGPLv3) licence option, and
  SessionBar's own code is licensed GPL-3.0. JUCE's binaries are upstream
  open-source components included in the signed installer; they are not
  signed with this project's SignPath subscription.
- **Inno Setup** — builds the installer from this repository's installer
  script.
- Windows system libraries are included as System Libraries.

## Privacy

This program will not transfer any information to other networked systems
unless specifically requested by the user or the person installing or
operating it.

Specifically for SessionBar: the session clocks and countdowns are computed
entirely on the local machine from the system clock and the operating
system's time-zone database — no market data or any other content is
fetched, and SessionBar collects no telemetry. Its only network traffic is
its self-updater: it queries
`api.github.com/repos/Corporal8065/SessionBar/releases/latest` (on launch via
the updater poll, and on menu request) and downloads a new
`SessionBar-Setup.exe` from GitHub only when the user chooses to update
(verified by size and SHA-256 before installing). GitHub's own privacy
policy applies to those requests
(https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).
